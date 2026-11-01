#ifndef BUDDYGUARD_ROSTER_H
#define BUDDYGUARD_ROSTER_H
/* Start scanner once, poll from main; tag builds are no-ops.
 * 辅导员启动扫描并由主循环轮询；标签端为空实现。 */
int bg_roster_start(void);
void bg_roster_poll(void);
#endif
