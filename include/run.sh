#!/bin/sh

[ -f "/mnt/mtd/passwd" ] && mount --bind /mnt/mtd/passwd /etc/passwd

echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"

source /usr/bin/gpio.sh
setup_all_gpio
wifi_led red

ifconfig eth0 up
ifconfig eth0 192.168.1.10

telnetd &

# copy etc as writable
cp -Rdpf /etc /var/etc
mount --bind /var/etc /etc

for P in /usr /mnt/mtd ; do
if [ -e "${P}/etc" ]; then
  cp -Rdpf ${P}/etc/* /etc
fi
done

if command -v dropbear >/dev/null; then
  mkdir -p /mnt/mtd/dropbear
  dropbear -R

  if [ "$?" = 0 ]; then
    echo "dropbear enabled"
    killall telnetd
  fi
fi

for DIR in lock run fat32_0 cloud wifi; do
  mkdir -p /var/$DIR
done
cp -f /usr/cloud/states /var/cloud/states

for FILE in hal hw_crypto media audio sensor i2s; do
  insmod /ko/${FILE}.ko
done

for FILE in wifi_pow rtl8188fu bit1628a rtc8563; do
  insmod /ko/extdrv/${FILE}.ko
done

sleep 1
mdev -s

# update hostname, read config line 4 straight to the UDID
UDID=$(dd if=/dev/mtdblock6 skip=324 count=12 bs=1 2>/dev/null)
[ -z "${UDID}" ] && UDID="000000000000"
echo "IDS7938${UDID:8:4}" > /proc/sys/kernel/hostname

# Prepare writable WiFi runtime files. Final network mode is selected after
# Sofia warmup because Sofia can disrupt wlan0.
cp /usr/sbin/wifi_conf/* /var/wifi/
cp /usr/sbin/hostapd.conf /var/wifi

wifi_led off

ln -s /mnt/mtd/Config/resolv.conf /var/resolv.conf

# increase network buffer
echo 1084576 > /proc/sys/net/core/rmem_max
echo 1084576 > /proc/sys/net/core/wmem_max
echo 1 > /proc/sys/net/ipv4/conf/all/arp_ignore
echo 2 > /proc/sys/net/ipv4/conf/all/arp_announce

echo 3 > /proc/sys/vm/drop_caches; free

# stop heartbeat until full start
touch /tmp/heartbeat.lock

# cron
CRONTABS="/var/spool/cron/crontabs"
mkdir -p ${CRONTABS}
cat << EOF >> ${CRONTABS}/root
15 3 * * 6 reboot
*/10 * * * * /usr/bin/heartbeat.sh
0 * * * * ntpd -q -p pool.ntp.org
* * * * * dmesg -c | grep -v RTL871X >> /var/messages
EOF
if [ -f "/mnt/mtd/crontab" ]; then
  cat /mnt/mtd/crontab >> ${CRONTABS}/root
fi
crond -b

# get settings from uboot
RUN_SOFIA=$(strings /dev/mtdblock1 | grep -E "^sofia=" | cut -d '=' -f2)

if [ -z "${RUN_SOFIA}" ] || [ "${RUN_SOFIA}" != "0" ]; then
  timeout -t 180 /usr/bin/Sofia_temp.sh
fi

WIFI_MODE=$(/usr/bin/wifi_mode.sh)
if [ "$WIFI_MODE" = "ap" ]; then
  wifi_led off
  /usr/bin/ap_start.sh
else
  rm -f /tmp/wifi-station-ready
  /usr/bin/wifi_station_manager.sh &
  WIFI_WAIT=0
  while [ "$WIFI_WAIT" -lt 35 ] && [ ! -f /tmp/wifi-station-ready ]; do
    sleep 1
    WIFI_WAIT=$((WIFI_WAIT + 1))
  done
fi

# Sofia_temp loads a watchdog configuration only during hardware warmup and
# then unloads it. Reload the real watchdog in stoppable mode for the daemon.
WATCHDOG_MODULE=/ko/extdrv/goke_wdt.ko
if grep -q '^goke_wdt ' /proc/modules 2>/dev/null; then
  echo "Reloading warmup watchdog in production mode"
  /sbin/rmmod goke_wdt || echo "WARNING: unable to unload warmup watchdog"
fi
if ! grep -q '^goke_wdt ' /proc/modules 2>/dev/null; then
  # The module metadata reverses these labels on this GK7102S build:
  # init_mode=2 only raises an IRQ, while init_mode=4 performs a hardware reset.
  if /sbin/insmod "${WATCHDOG_MODULE}" init_mode=4 soft_noboot=0 nowayout=0 tmr_atboot=0 tmr_margin=30; then
    sleep 1
    /sbin/mdev -s
  else
    echo "WARNING: unable to load ${WATCHDOG_MODULE}; app watchdog remains active"
  fi
fi
if [ ! -c /dev/watchdog ]; then
  echo "WARNING: /dev/watchdog unavailable; app watchdog remains active"
fi

# update hostname after Sofia run
echo "IDS7938${UDID:8:4}" > /proc/sys/kernel/hostname

if [ ! -f "/mnt/mtd/sip_media.conf" ] && [ -f "/etc/sip_media.conf.default" ]; then
  cp /etc/sip_media.conf.default /mnt/mtd/sip_media.conf
fi
if [ ! -x "/usr/bin/wibox-media-daemon" ]; then
  echo "wibox-media-daemon missing"
  exit 1
fi
/usr/bin/app_watchdog.sh wibox-media-daemon /usr/bin/wibox-media-daemon &

if [ -x "/mnt/mtd/post.sh" ]; then
  /mnt/mtd/post.sh
fi

# remove lock if present
rm -f /tmp/heartbeat.lock

wifi_led blue
