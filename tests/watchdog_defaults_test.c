#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_config(char *path, const char *contents) {
    int fd = mkstemp(path);
    FILE *fp;

    if (fd < 0) {
        return -1;
    }
    fp = fdopen(fd, "w");
    if (!fp) {
        close(fd);
        return -1;
    }
    if (fputs(contents, fp) == EOF || fclose(fp) != 0) {
        return -1;
    }
    return 0;
}

int main(void) {
    char legacy_path[] = "/tmp/wibox-config-legacy-XXXXXX";
    char override_path[] = "/tmp/wibox-config-override-XXXXXX";
    wibox_config_t config;
    int failed = 0;

    if (write_config(legacy_path, "sip_port=5090\n") != 0 ||
        config_load(legacy_path, &config) != 0) {
        return 1;
    }
    if (config.sip_port != 5090 || config.intercom_reopen_guard_ms != 0 ||
        config.hardware_watchdog_enabled != 1 ||
        strcmp(config.hardware_watchdog_device, "/dev/watchdog") != 0 ||
        config.hardware_watchdog_timeout_seconds != 30 ||
        config.hardware_watchdog_feed_interval_seconds != 5) {
        fprintf(stderr, "legacy config did not inherit watchdog defaults\n");
        failed = 1;
    }
    unlink(legacy_path);

    if (write_config(override_path,
                     "intercom_reopen_guard_ms=3000\n"
                     "hardware_watchdog_enabled=0\n"
                     "hardware_watchdog_device=/tmp/test-watchdog\n"
                     "hardware_watchdog_timeout_seconds=60\n"
                     "hardware_watchdog_feed_interval_seconds=10\n") != 0 ||
        config_load(override_path, &config) != 0) {
        return 1;
    }
    if (config.intercom_reopen_guard_ms != 3000 ||
        config.hardware_watchdog_enabled != 0 ||
        strcmp(config.hardware_watchdog_device, "/tmp/test-watchdog") != 0 ||
        config.hardware_watchdog_timeout_seconds != 60 ||
        config.hardware_watchdog_feed_interval_seconds != 10) {
        fprintf(stderr, "explicit watchdog overrides were not applied\n");
        failed = 1;
    }
    unlink(override_path);
    return failed;
}
