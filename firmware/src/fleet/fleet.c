/* SPDX-License-Identifier: Apache-2.0 */
/* Fleet snapshots only; no network or radio work in the metrics callback. */
#include <zephyr/app_version.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <soc/usb_serial_jtag_struct.h>
#include "memfault/core/platform/device_info.h"
#include "memfault/metrics/metrics.h"
#include "imu.h"
#include "radio_link.h"
#include "roster.h"

static atomic_t usb_host_active;

void memfault_platform_get_device_info(sMemfaultDeviceInfo *info)
{
	*info = (sMemfaultDeviceInfo){
		.device_serial = CONFIG_BUDDYGUARD_DEVICE_SERIAL,
		.software_type = IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) ?
			"buddyguard-counselor" : "buddyguard-tag",
		.software_version = APP_VERSION_STRING,
		/* Cloud hardware variants distinguish the two product roles, even
		 * though both currently use the same development kit. */
		.hardware_version = IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) ?
			"xiao-esp32s3-wio-counselor-v1" : "xiao-esp32s3-wio-tag-v1",
	};
}

/* Observe USB SOF progression, not a guessed battery percentage or VBUS voltage.
 * Called in main context so the heartbeat callback never sleeps. USB host activity
 * alone does not identify the supply when a battery/external source is attached.
 */
void bg_fleet_poll(void)
{
	uint32_t frame = USB_SERIAL_JTAG.fram_num.sof_frame_index;
	k_sleep(K_MSEC(3));
	atomic_set(&usb_host_active,
		frame != USB_SERIAL_JTAG.fram_num.sof_frame_index);
}

void memfault_metrics_heartbeat_collect_data(void)
{
	/* Snapshot existing services; the SDK supplies CPU/heap/thread metrics.
	 * 这里只补产品指标；不在 SDK 心跳回调里联网、等待或操作无线电。 */
	struct bg_radio_stats radio = bg_radio_get_stats();
	struct bg_imu_snapshot imu = bg_imu_get();
	bool usb = atomic_get(&usb_host_active) != 0;
	MEMFAULT_METRIC_SET_UNSIGNED(bg_uptime_s, (uint32_t)(k_uptime_get() / 1000));
	MEMFAULT_METRIC_SET_UNSIGNED(bg_lora_rx_total, radio.rx_valid);
	MEMFAULT_METRIC_SET_UNSIGNED(bg_lora_failed_total, radio.tx_exhausted);
	MEMFAULT_METRIC_SET_UNSIGNED(bg_lora_queue_drops_total, radio.queue_drops);
	MEMFAULT_METRIC_SET_UNSIGNED(bg_missing_events_total, bg_roster_missing_count());
	MEMFAULT_METRIC_SET_UNSIGNED(bg_imu_available, imu.available);
	MEMFAULT_METRIC_SET_UNSIGNED(bg_motion, bg_imu_motion());
	MEMFAULT_METRIC_SET_UNSIGNED(bg_image_confirmed, boot_is_img_confirmed());
	MEMFAULT_METRIC_SET_UNSIGNED(bg_usb_host_active, usb);
	MEMFAULT_METRIC_SET_STRING(bg_power_status, usb ? "USB_HOST_ACTIVE" : "UNKNOWN");
	MEMFAULT_METRIC_SET_UNSIGNED(bg_battery_soc_available, 0);
	/* Report the running firmware policy, not an assumed cloud cohort assignment.
	 * A/B only changes diagnostic sampling; BLE presence and alarms are unchanged. */
	unsigned interval = CONFIG_MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS;
	MEMFAULT_METRIC_SET_UNSIGNED(bg_report_interval_s, interval);
	MEMFAULT_METRIC_SET_STRING(bg_ab_variant, interval == 15 ? "B_FAST" :
		(interval == 60 ? "A_CONTROL" : "CUSTOM"));
}
