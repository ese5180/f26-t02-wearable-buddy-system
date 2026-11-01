/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_PRESENCE_H
#define BUDDYGUARD_PRESENCE_H
#include <stdbool.h>
#include <stdint.h>
/* BLE service. Init/tick/status are main/shell context; callbacks stay internal.
 * BLE 服务：启动、周期更新与状态查询供主循环／命令层使用，回调留在模块内。 */
struct bg_presence_status {
	uint16_t sequence;
	bool advertising, connected, maintenance, heartbeat;
};
int bg_presence_init(void);
int bg_presence_tick(void);
/* Admission deadline, not forced disconnection. / 设置接入期限，不强制断开已有连接。 */
void bg_presence_maintenance(unsigned seconds);
void bg_presence_heartbeat(bool enabled);
struct bg_presence_status bg_presence_status(void);
#endif
