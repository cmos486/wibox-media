#ifndef HARDWARE_WATCHDOG_H
#define HARDWARE_WATCHDOG_H

int hardware_watchdog_start(int enabled,
                            const char* device,
                            int timeout_seconds,
                            int feed_interval_seconds);
void hardware_watchdog_heartbeat(void);
void hardware_watchdog_stop(int disarm);

#endif
