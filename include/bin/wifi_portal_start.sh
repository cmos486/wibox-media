#!/bin/sh

# Start the minimal Wi-Fi provisioning UI after the AP has an address.
# The BusyBox httpd applet is used because production images do not carry
# another web server.
WEB_ROOT=/var/wifi/www
SOURCE_ROOT=/usr/www/wifi

# include/www is installed below the read-only /usr mount.  Copy it to /var
# so the CGI can live alongside the other runtime Wi-Fi files.
if [ -d "${SOURCE_ROOT}" ]; then
    mkdir -p "${WEB_ROOT}"
    cp -R "${SOURCE_ROOT}/." "${WEB_ROOT}/"
fi

[ -f "${WEB_ROOT}/index.html" ] || exit 0

killall -q httpd 2>/dev/null || true
busybox httpd -p 80 -h "${WEB_ROOT}" >/dev/null 2>&1 &

# Rebind SSH after the station-to-AP transition. The shared helper waits for
# the old listener to exit and verifies the replacement process with retries.
DROPBEAR_RESTART=${WIFI_DROPBEAR_RESTART:-/usr/bin/dropbear_restart.sh}
if [ -x "$DROPBEAR_RESTART" ]; then
    "$DROPBEAR_RESTART" || true
fi
