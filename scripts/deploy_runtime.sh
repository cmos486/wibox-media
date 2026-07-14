#!/bin/sh
set -eu

WIBOX_IP="${WIBOX_IP:-192.168.0.196}"
WIBOX_USER="${WIBOX_USER:-root}"
WIBOX_PASS="${WIBOX_PASS:-qv2008}"
LOCAL_BIN="${LOCAL_BIN:-include/bin/wibox-media-daemon}"
REMOTE_DIR="${REMOTE_DIR:-/tmp/wibox-media-test}"
SSH_OPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

if [ ! -x "${LOCAL_BIN}" ]; then
  echo "[!] Missing local daemon: ${LOCAL_BIN}" >&2
  exit 1
fi

LOCAL_MD5=$(md5sum "${LOCAL_BIN}" | cut -d" " -f1)
echo "[*] Uploading ${LOCAL_BIN} to ${WIBOX_USER}@${WIBOX_IP}:${REMOTE_DIR}"

base64 "${LOCAL_BIN}" | sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" "
set -eu
mkdir -p '${REMOTE_DIR}/bin'
base64 -d > '${REMOTE_DIR}/bin/wibox-media-daemon.new'
chmod +x '${REMOTE_DIR}/bin/wibox-media-daemon.new'
mv '${REMOTE_DIR}/bin/wibox-media-daemon.new' '${REMOTE_DIR}/bin/wibox-media-daemon'
md5sum '${REMOTE_DIR}/bin/wibox-media-daemon'
"

REMOTE_MD5=$(sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" "md5sum '${REMOTE_DIR}/bin/wibox-media-daemon' | cut -d' ' -f1")
if [ "${REMOTE_MD5}" != "${LOCAL_MD5}" ]; then
  echo "[!] Checksum mismatch: local=${LOCAL_MD5} remote=${REMOTE_MD5}" >&2
  exit 2
fi

echo "[*] Restarting temporary daemon"
sshpass -p "${WIBOX_PASS}" ssh ${SSH_OPTS} "${WIBOX_USER}@${WIBOX_IP}" "
set -eu
WATCHDOG_PIDS=''
for PROCESS in /proc/[0-9]*; do
  PROCESS_NAME=\$(cat \"\$PROCESS/comm\" 2>/dev/null || true)
  [ \"\$PROCESS_NAME\" = 'app_watchdog.sh' ] || continue
  [ -r \"\$PROCESS/cmdline\" ] || continue
  COMMAND=\$(tr '\\000' ' ' < \"\$PROCESS/cmdline\" 2>/dev/null || true)
  case \"\$COMMAND\" in
    *'app_watchdog.sh wibox-media-daemon '*)
      WATCHDOG_PIDS=\"\$WATCHDOG_PIDS \${PROCESS##*/}\"
      ;;
  esac
done
if [ -n \"\$WATCHDOG_PIDS\" ]; then
  echo \"[*] Suspending installed app watchdog:\$WATCHDOG_PIDS\"
  kill \$WATCHDOG_PIDS 2>/dev/null || true
  sleep 1
  for PID in \$WATCHDOG_PIDS; do
    [ -d \"/proc/\$PID\" ] && kill -9 \"\$PID\" 2>/dev/null || true
  done
fi
PIDS=''
for PROCESS in /proc/[0-9]*; do
  EXECUTABLE=\$(readlink \"\$PROCESS/exe\" 2>/dev/null || true)
  EXECUTABLE=\${EXECUTABLE% (deleted)}
  [ \"\${EXECUTABLE##*/}\" = 'wibox-media-daemon' ] || continue
  PIDS=\"\$PIDS \${PROCESS##*/}\"
done
if [ -n \"\$PIDS\" ]; then
  kill \$PIDS 2>/dev/null || true
fi
sleep 2
PIDS=''
for PROCESS in /proc/[0-9]*; do
  EXECUTABLE=\$(readlink \"\$PROCESS/exe\" 2>/dev/null || true)
  EXECUTABLE=\${EXECUTABLE% (deleted)}
  [ \"\${EXECUTABLE##*/}\" = 'wibox-media-daemon' ] || continue
  PIDS=\"\$PIDS \${PROCESS##*/}\"
done
if [ -n \"\$PIDS\" ]; then
  kill -9 \$PIDS 2>/dev/null || true
fi
cd '${REMOTE_DIR}'
export LD_LIBRARY_PATH='${REMOTE_DIR}/lib:/usr/lib:/lib'
mkdir -p /var/log
nohup ./bin/wibox-media-daemon /mnt/mtd/sip_media.conf >/var/log/wibox-media-daemon.log 2>&1 &
sleep 5
ps | grep -E 'wibox-media-daemon' | grep -v grep
tail -60 /var/log/wibox-media-daemon.log
"
