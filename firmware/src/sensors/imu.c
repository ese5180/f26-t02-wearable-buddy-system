/* SPDX-License-Identifier: Apache-2.0 */
/* Owns optional I2C sampling; consumers read a locked snapshot, never the sensor bus.
 * 负责可选 I2C 采样；其他模块读取加锁快照，不直接访问传感器。
 */
#include <errno.h>
#include <zephyr/kernel.h>
#include "imu.h"

#if defined(CONFIG_BUDDYGUARD_IMU)
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include "radio_link.h"

LOG_MODULE_REGISTER(bg_imu, LOG_LEVEL_INF);
K_MUTEX_DEFINE(snapshot_lock);
K_SEM_DEFINE(imu_started, 0, 1);
static struct bg_imu_snapshot latest = {.enabled = true, .last_error = -EAGAIN};
static struct bg_motion_filter filter;
static struct bg_impact_detector impact_detector;

/* Optional logic-analyzer marker: high for CONFIG_BUDDYGUARD_IMPACT_MARKER_MS per impact.
 * 可选的逻辑分析仪标记脚：每次冲击拉高一段固定时间。 */
#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), impact_gpios)
static const struct gpio_dt_spec marker = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), impact_gpios);
static void marker_off(struct k_work *work)
{
	ARG_UNUSED(work);
	(void)gpio_pin_set_dt(&marker, 0);
}
static K_WORK_DELAYABLE_DEFINE(marker_work, marker_off);
static void marker_init(void)
{
	if (gpio_is_ready_dt(&marker)) {
		(void)gpio_pin_configure_dt(&marker, GPIO_OUTPUT_INACTIVE);
	}
}
static void marker_pulse(void)
{
	if (gpio_is_ready_dt(&marker)) {
		(void)gpio_pin_set_dt(&marker, 1);
		k_work_reschedule(&marker_work, K_MSEC(CONFIG_BUDDYGUARD_IMPACT_MARKER_MS));
	}
}
#else
static void marker_init(void) {}
static void marker_pulse(void) {}
#endif

/* LSM6DSO wake-up detector (registers from ST's lsm6dso_reg.h). It compares every
 * 417 Hz accelerometer sample with the previous one (slope filter) and latches an
 * event in WAKE_UP_SRC until we read it, so no INT wire is needed.
 * 用 IMU 内置的 wake-up 检测：每个 417 Hz 样本做 slope，超阈值就锁存，我们轮询读取。 */
#define LSM6DSO_WAKE_UP_SRC 0x1b /* bit3 WU_IA, bit2 X_WU, bit1 Y_WU, bit0 Z_WU */
#define LSM6DSO_TAP_CFG0    0x56 /* bit6 INT_CLR_ON_READ, bit4 SLOPE_FDS, bit0 LIR */
#define LSM6DSO_TAP_CFG2    0x58 /* bit7 INTERRUPTS_ENABLE */
#define LSM6DSO_WAKE_UP_THS 0x5b /* bits5:0 WK_THS */
#define LSM6DSO_WAKE_UP_DUR 0x5c /* bit4 WAKE_THS_W (0: 1 LSB = full scale / 64) */
#define LSM6DSO_MD1_CFG     0x5e /* bit5 INT1_WU */
/* +/-4 g full scale: 1 LSB = 4000 mg / 64 = 62.5 mg. */
#define WAKE_THS_LSB  CLAMP((CONFIG_BUDDYGUARD_IMPACT_THRESHOLD_MG * 64 + 2000) / 4000, 1, 63)

static const struct i2c_dt_spec *active_bus;
static bool wakeup_ready;

static void wakeup_init(void)
{
	int rc = i2c_reg_update_byte_dt(active_bus, LSM6DSO_TAP_CFG0, 0x51, 0x41);
	rc = rc ? rc : i2c_reg_update_byte_dt(active_bus, LSM6DSO_WAKE_UP_DUR, 0x70, 0x00);
	rc = rc ? rc : i2c_reg_update_byte_dt(active_bus, LSM6DSO_WAKE_UP_THS, 0x3f, WAKE_THS_LSB);
	rc = rc ? rc : i2c_reg_update_byte_dt(active_bus, LSM6DSO_MD1_CFG, 0x20, 0x20);
	rc = rc ? rc : i2c_reg_update_byte_dt(active_bus, LSM6DSO_TAP_CFG2, 0x80, 0x80);
	uint8_t stale;
	rc = rc ? rc : i2c_reg_read_byte_dt(active_bus, LSM6DSO_WAKE_UP_SRC, &stale);
	wakeup_ready = rc == 0;
	if (wakeup_ready) {
		LOG_INF("IMU wake-up detector: threshold %d mg (WK_THS=%d) at 417 Hz",
			CONFIG_BUDDYGUARD_IMPACT_THRESHOLD_MG, WAKE_THS_LSB);
	} else {
		LOG_WRN("IMU wake-up detector setup failed rc=%d; using polled samples only", rc);
	}
}

static uint8_t wakeup_axes(void)
{
	uint8_t src;
	if (!wakeup_ready || i2c_reg_read_byte_dt(active_bus, LSM6DSO_WAKE_UP_SRC, &src) ||
	    !(src & BIT(3))) {
		return 0;
	}
	/* Register order is z=bit0, y=bit1, x=bit2; ours is x=bit0, y=bit1, z=bit2. */
	return ((src & BIT(2)) ? BIT(0) : 0) | ((src & BIT(1)) ? BIT(1) : 0) |
	       ((src & BIT(0)) ? BIT(2) : 0);
}

static void check_impact(struct bg_imu_snapshot *sample, uint8_t hw_axes)
{
	struct bg_impact impact = bg_impact_update(&impact_detector, (uint32_t)sample->sampled_ms,
		sample->accel_mg, CONFIG_BUDDYGUARD_IMPACT_THRESHOLD_MG,
		CONFIG_BUDDYGUARD_IMPACT_LOCKOUT_MS, hw_axes);
	if (!impact.detected) {
		return;
	}
	/* Marker first: the analyzer measures reaction time from the I2C read. */
	marker_pulse();
	++sample->impacts;
	sample->last_impact_mg = impact.delta_mg;
	int rc = bg_radio_sos();
	LOG_WRN("IMPACT src=%s axis=%c polled_delta_mg=%d (threshold %d); SOS queue result=%d",
		impact.hardware ? "imu" : "polled", "xyz"[impact.axis], impact.delta_mg,
		CONFIG_BUDDYGUARD_IMPACT_THRESHOLD_MG, rc);
}

static const struct {
	struct i2c_dt_spec bus;
	const struct device *sensor;
} candidates[] = {
	{I2C_DT_SPEC_GET(DT_NODELABEL(imu_high)), DEVICE_DT_GET(DT_NODELABEL(imu_high))},
	{I2C_DT_SPEC_GET(DT_NODELABEL(imu_low)), DEVICE_DT_GET(DT_NODELABEL(imu_low))},
};

static const struct device *probe(struct bg_imu_snapshot *sample)
{
	for (unsigned i = 0; i < ARRAY_SIZE(candidates); ++i) {
		uint8_t whoami = 0;
		if (!i2c_is_ready_dt(&candidates[i].bus)) {
			continue;
		}
		int rc = i2c_reg_read_byte_dt(&candidates[i].bus, 0x0f, &whoami);
		if (rc || whoami != 0x6c) {
			continue;
		}
		sample->address = candidates[i].bus.addr;
		sample->whoami = whoami;
		LOG_INF("LSM6DSO detected: I2C address=0x%02x WHO_AM_I=0x%02x",
			sample->address, whoami);
		/* Both possible addresses are deferred; only initialize the detected one. */
		rc = device_init(candidates[i].sensor);
		if (rc != 0 && rc != -EALREADY) {
			LOG_ERR("LSM6DSO driver initialization failed: %d", rc);
			continue;
		}
		if (device_is_ready(candidates[i].sensor)) {
			active_bus = &candidates[i].bus;
			return candidates[i].sensor;
		}
	}
	return NULL;
}

static void publish(const struct bg_imu_snapshot *sample)
{
	k_mutex_lock(&snapshot_lock, K_FOREVER);
	latest = *sample;
	k_mutex_unlock(&snapshot_lock);
}

static void worker(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	k_sem_take(&imu_started, K_FOREVER);
	marker_init();
	struct bg_imu_snapshot sample = {.enabled = true, .last_error = -ENODEV};
	const struct device *sensor = NULL;
	uint8_t previous_motion = BG_MOTION_UNKNOWN;
	int64_t last_log = -10000;
	for (;;) {
		if (sensor == NULL) {
			sensor = probe(&sample);
			if (sensor == NULL) {
				publish(&sample);
				if (k_uptime_get() - last_log >= 10000) {
					LOG_WRN("IMU unavailable at 0x6b/0x6a; check 3V3/GND/SDA=D4/SCL=D5");
					last_log = k_uptime_get();
				}
				k_sleep(K_SECONDS(2));
				continue;
			}
			LOG_INF("IMU sampling: accel 417 Hz / gyro 52 Hz ODR, 25 Hz polling, accel +/-4g, gyro +/-250dps");
			wakeup_init();
		}
		struct sensor_value accel[3], gyro[3];
		uint8_t status;
		int rc = i2c_reg_read_byte_dt(active_bus, 0x1e, &status);
		if (!rc && (status & 3) != 3) {
			/* Both accel and gyro must have a new sample. Snapshot TTL handles
			 * a stalled sensor instead of treating repeated registers as fresh.
			 */
			k_sleep(K_MSEC(40));
			continue;
		}
		if (!rc) {
			rc = sensor_sample_fetch(sensor);
		}
		if (!rc) {
			rc = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel);
		}
		if (!rc) {
			rc = sensor_channel_get(sensor, SENSOR_CHAN_GYRO_XYZ, gyro);
		}
		uint8_t hw_axes = rc ? 0 : wakeup_axes();
		if (rc) {
			sample.available = false;
			sample.motion = BG_MOTION_UNKNOWN;
			sample.last_error = rc;
			bg_motion_reset(&filter);
			bg_impact_reset(&impact_detector);
			publish(&sample);
			if (k_uptime_get() - last_log >= 2000) {
				LOG_WRN("IMU read failed rc=%d; motion=UNKNOWN", rc);
				last_log = k_uptime_get();
			}
			k_sleep(K_MSEC(200));
			continue;
		}
		for (unsigned i = 0; i < 3; ++i) {
			sample.accel_mg[i] = sensor_value_to_micro(&accel[i]) * 1000LL / SENSOR_G;
			sample.gyro_mrad_s[i] = sensor_value_to_milli(&gyro[i]);
		}
		sample.sampled_ms = k_uptime_get();
		sample.motion = bg_motion_update(&filter, (uint32_t)sample.sampled_ms,
			sample.accel_mg, sample.gyro_mrad_s);
		sample.available = true;
		sample.last_error = 0;
		++sample.samples;
		check_impact(&sample, hw_axes);
		publish(&sample);
		if (sample.motion != previous_motion || sample.sampled_ms - last_log >= 2000) {
			LOG_INF("IMU motion=%s accel_mg=(%d,%d,%d) gyro_mrad_s=(%d,%d,%d) samples=%u",
				bg_motion_name(sample.motion), sample.accel_mg[0], sample.accel_mg[1],
				sample.accel_mg[2], sample.gyro_mrad_s[0], sample.gyro_mrad_s[1],
				sample.gyro_mrad_s[2], sample.samples);
			last_log = sample.sampled_ms;
			previous_motion = sample.motion;
		}
		k_sleep(K_MSEC(40));
	}
}

K_THREAD_DEFINE(bg_imu_thread, 3072, worker, NULL, NULL, NULL, 7, 0, 0);

void bg_imu_start(void)
{
	k_sem_give(&imu_started);
}

struct bg_imu_snapshot bg_imu_get(void)
{
	k_mutex_lock(&snapshot_lock, K_FOREVER);
	struct bg_imu_snapshot sample = latest;
	k_mutex_unlock(&snapshot_lock);
	if (!sample.samples || k_uptime_get() - sample.sampled_ms > 1000) {
		sample.available = false;
		sample.motion = BG_MOTION_UNKNOWN;
		if (sample.samples && sample.last_error == 0) {
			sample.last_error = -ETIMEDOUT;
		}
	}
	return sample;
}
#else
void bg_imu_start(void) {}
struct bg_imu_snapshot bg_imu_get(void)
{
	return (struct bg_imu_snapshot) {.last_error = -ENOTSUP};
}
#endif

enum bg_motion bg_imu_motion(void)
{
	struct bg_imu_snapshot sample = bg_imu_get();
	return sample.available ? sample.motion : BG_MOTION_UNKNOWN;
}
