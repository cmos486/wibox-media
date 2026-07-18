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

# Rebind SSH after the station-to-AP transition.  On this platform the
# early Dropbear listener can remain unreachable after wlan0 is reconfigured.
killall -q dropbear 2>/dev/null || true
if command -v dropbear >/dev/null 2>&1; then
    dropbear -R >/dev/null 2>&1 || true
elif [ -x /sbin/dropbear ]; then
    /sbin/dropbear -R >/dev/null 2>&1 || true
fi
