/* SPDX-License-Identifier: Apache-2.0 */
/* One worker owns cloud traffic. BLE/LoRa and the watchdog keep running in main.
 * 板子直接走 Wi-Fi；复用 SDK 的 TLS/HTTP，复用 MCUboot 的验签与试运行。
 */
#include <errno.h>
#include <string.h>
#include <time.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/sntp.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/clock.h>
#include "memfault/components.h"
#include "memfault/ports/zephyr/http.h"
#include "presence.h"
#include "wifi_fleet.h"

LOG_MODULE_REGISTER(bg_wifi, LOG_LEVEL_INF);
static struct { char ssid[33], password[65]; } profile;
static struct net_if *iface;
static struct net_mgmt_event_callback ip_event;
static bool ready;
static atomic_t provisioned;
static K_SEM_DEFINE(wake, 0, 1);
static K_SEM_DEFINE(got_ip, 0, 1);
static struct flash_img_context image;
static size_t expected, received;
static unsigned progress;
static bool downloading;
static uint8_t download_buf[1024];

static int load_profile(const char *key, size_t len, settings_read_cb read, void *arg)
{
	if (strcmp(key, "network") || len != sizeof(profile)) { return -EINVAL; }
	if (read(arg, &profile, sizeof(profile)) != sizeof(profile)) { return -EIO; }
	profile.ssid[32] = profile.password[64] = 0;
	atomic_set(&provisioned, profile.ssid[0] && profile.password[0]);
	return 0;
}
SETTINGS_STATIC_HANDLER_DEFINE(bg_wifi, "bg_wifi", NULL, load_profile, NULL, NULL);

static void on_ip(struct net_mgmt_event_callback *cb, uint64_t event, struct net_if *net)
{
	ARG_UNUSED(cb); ARG_UNUSED(event);
	if (net == iface) { k_sem_give(&got_ip); }
}

static int connect_wifi(void)
{
	struct wifi_iface_status state = {0};
	int rc = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &state, sizeof(state));
	if (!rc && state.state == WIFI_STATE_COMPLETED && net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED)) {
		return 0;
	}
	struct wifi_connect_req_params req = {
		.ssid = (uint8_t *)profile.ssid, .ssid_length = strlen(profile.ssid),
		.psk = (uint8_t *)profile.password, .psk_length = strlen(profile.password),
		.security = WIFI_SECURITY_TYPE_PSK, .band = WIFI_FREQ_BAND_2_4_GHZ,
		.channel = WIFI_CHANNEL_ANY, .mfp = WIFI_MFP_OPTIONAL, .timeout = 30,
	};
	k_sem_reset(&got_ip);
	rc = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &req, sizeof(req));
	if (rc && rc != -EALREADY) { return rc; }
	return k_sem_take(&got_ip, K_SECONDS(40));
}

static bool update_available(const sMemfaultOtaInfo *info, void *unused)
{
	ARG_UNUSED(unused);
	/* Reserve coredump + trailer + offset-swap sector; never overwrite primary. */
	expected = info->size;
	received = progress = 0;
	if (!boot_is_img_confirmed() || expected < 32 || expected > 0x150000 - 0x30000 - 4096 ||
	    bg_presence_status().connected) { return false; }
	bg_presence_maintenance(600);
	downloading = true;
	LOG_INF("Wi-Fi OTA downloading %u bytes", (unsigned)expected);
	return flash_img_init(&image) == 0;
}

static bool update_data(void *buf, size_t len, void *unused)
{
	ARG_UNUSED(unused);
	if (len > expected - received || flash_img_buffered_write(&image, buf, len, false)) {
		return false;
	}
	received += len;
	unsigned percent = received * 100 / expected;
	if (percent / 10 != progress / 10) {
		LOG_INF("Wi-Fi OTA %u%%", percent);
		progress = percent;
	}
	return true;
}

static bool update_complete(void *unused)
{
	ARG_UNUSED(unused);
	/* The final partial block MUST be flushed before requesting a test boot.
	 * 下载完整 != 已安装；重启后仍由 MCUboot 验签，健康逻辑自行确认。 */
	if (received != expected || flash_img_buffered_write(&image, NULL, 0, true) ||
	    boot_request_upgrade(BOOT_UPGRADE_TEST)) { return false; }
	LOG_INF("Wi-Fi OTA download complete; test boot requested");
	MEMFAULT_REBOOT_MARK_RESET_IMMINENT(kMfltRebootReason_FirmwareUpdate);
	memfault_platform_reboot();
	return false;
}

static void cloud_cycle(void)
{
	/* Upload pending crashes before OTA can touch the secondary slot. */
	int rc = memfault_zephyr_port_post_data();
	LOG_INF("Wi-Fi telemetry upload result=%d", rc);
	size_t core_size;
	if (rc || !boot_is_img_confirmed() || memfault_coredump_has_valid_coredump(&core_size)) {
		return;
	}
	sMemfaultOtaUpdateHandler handler = {
		.buf = download_buf, .buf_len = sizeof(download_buf),
		.handle_update_available = update_available, .handle_data = update_data,
		.handle_download_complete = update_complete,
	};
	memset(&image, 0, sizeof(image));
	rc = memfault_zephyr_port_ota_update(&handler);
	/* Failed/interrupted downloads never request a swap; next attempt starts over. */
	if (image.flash_area) { flash_area_close(image.flash_area); image.flash_area = NULL; }
	if (downloading) { bg_presence_maintenance(0); downloading = false; }
	LOG_INF("Wi-Fi OTA check result=%d (0=up-to-date)", rc);
}

static void worker(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	k_sem_take(&wake, K_FOREVER);
	while (true) {
		if (atomic_get(&provisioned)) {
			int rc = connect_wifi();
			if (!rc) {
				struct sntp_time now;
				/* Keep TLS hostname and certificate validation enabled. */
				if (!sntp_simple("time.cloudflare.com", 5000, &now)) {
					struct timespec ts = {.tv_sec = now.seconds};
					sys_clock_settime(SYS_CLOCK_REALTIME, &ts);
					cloud_cycle();
				} else { LOG_WRN("Time sync unavailable; HTTPS postponed"); }
			} else { LOG_WRN("Wi-Fi connection failed: %d", rc); }
		}
		k_sem_take(&wake, K_SECONDS(30));
	}
}
K_THREAD_DEFINE(bg_wifi_thread, 12288, worker, NULL, NULL, NULL, 8, 0, 0);

void bg_wifi_start(void)
{
	iface = net_if_get_first_wifi();
	if (!iface || settings_subsys_init() || settings_load() || memfault_zephyr_port_install_root_certs()) {
		LOG_ERR("Wi-Fi fleet initialization failed"); return;
	}
	net_mgmt_init_event_callback(&ip_event, on_ip, NET_EVENT_IPV4_DHCP_BOUND);
	net_mgmt_add_event_callback(&ip_event);
	ready = true;
	const struct net_linkaddr *mac = net_if_get_link_addr(iface);
	LOG_INF("Wi-Fi STA MAC %02X:%02X:%02X:%02X:%02X:%02X; configured=%u",
		mac->addr[0], mac->addr[1], mac->addr[2], mac->addr[3], mac->addr[4], mac->addr[5],
		(unsigned)atomic_get(&provisioned));
	k_sem_give(&wake);
}

static int cmd_wifi(const struct shell *sh, size_t argc, char **argv)
{
	if (!ready) { return -ENODEV; }
	if (argc == 2 && !strcmp(argv[1], "check")) { k_sem_give(&wake); return 0; }
	if (argc == 2 && !strcmp(argv[1], "status")) {
		const struct net_linkaddr *mac = net_if_get_link_addr(iface);
		shell_print(sh, "wifi configured=%u MAC=%02X:%02X:%02X:%02X:%02X:%02X",
			(unsigned)atomic_get(&provisioned), mac->addr[0], mac->addr[1], mac->addr[2],
			mac->addr[3], mac->addr[4], mac->addr[5]); return 0;
	}
	if (argc == 5 && !strcmp(argv[1], "provision")) {
		/* Host disables echo; history is disabled. Do not log credential arguments.
		 * Save first, activate on reboot: the worker never races credential changes. */
		char ssid[33] = {0}, password[65] = {0};
		size_t sl = strlen(argv[2]), pl = strlen(argv[3]);
		if (!sl || sl > 64 || (sl & 1) || pl < 16 || pl > 126 || (pl & 1) || strlen(argv[4]) != 32 ||
		    hex2bin(argv[2], sl, ssid, 32) != sl / 2 ||
		    hex2bin(argv[3], pl, password, 64) != pl / 2 ||
		    strlen(ssid) != sl / 2 || strlen(password) != pl / 2) { return -EINVAL; }
		struct { char ssid[33], password[65]; } next = {0};
		memcpy(next.ssid, ssid, sizeof(ssid)); memcpy(next.password, password, sizeof(password));
		int rc = memfault_zephyr_port_set_project_key(argv[4], 32);
		if (!rc) { rc = settings_save_one("bg_wifi/network", &next, sizeof(next)); }
		memset(password, 0, sizeof(password)); memset(&next, 0, sizeof(next));
		shell_print(sh, "Wi-Fi provision result=%d; reboot to apply", rc); return rc;
	}
	return -EINVAL;
}
SHELL_CMD_ARG_REGISTER(bg_wifi, NULL, "status | check | provision <ssid hex> <psk hex> <project key>",
	cmd_wifi, 2, 3);
