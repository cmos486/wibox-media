#!/bin/sh
set -eu

MODE="${1:-core}"
WIBOX_IP="${WIBOX_IP:-192.168.0.196}"
WIBOX_USER="${WIBOX_USER:-root}"
WIBOX_PASS="${WIBOX_PASS:-}"
SSH_OPTIONS="-o BatchMode=no -o ConnectTimeout=8 -o StrictHostKeyChecking=no"

if [ "${WIBOX_DEVICE_TEST:-0}" != "1" ]; then
    echo "Set WIBOX_DEVICE_TEST=1 to run acceptance against a real WiBox." >&2
    exit 2
fi

remote() {
    if [ -n "$WIBOX_PASS" ]; then
        command -v sshpass >/dev/null 2>&1 || {
            echo "sshpass is required when WIBOX_PASS is set" >&2
            exit 2
        }
        # shellcheck disable=SC2086
        sshpass -p "$WIBOX_PASS" ssh $SSH_OPTIONS "$WIBOX_USER@$WIBOX_IP" "$1"
    else
        # shellcheck disable=SC2086
        ssh $SSH_OPTIONS "$WIBOX_USER@$WIBOX_IP" "$1"
    fi
}

confirm() {
    prompt="$1"
    printf '%s [yes/no]: ' "$prompt"
    read -r answer
    [ "$answer" = "yes" ] || {
        echo "Acceptance failed: $prompt" >&2
        exit 1
    }
}

core_checks() {
    remote '
        test -x /usr/bin/wibox-media-daemon &&
        test -x /usr/bin/firmware_update &&
        test -x /usr/bin/app_watchdog.sh &&
        test -f /mnt/mtd/sip_media.conf &&
        test -f /etc/wibox-release &&
        count=0 &&
        for process in /proc/[0-9]*; do
            executable=$(readlink "$process/exe" 2>/dev/null || true)
            [ "${executable##*/}" = "wibox-media-daemon" ] || continue
            parent_pid=$(awk "{print \$4}" "$process/stat")
            parent_executable=$(readlink "/proc/$parent_pid/exe" 2>/dev/null || true)
            [ "${parent_executable##*/}" = "wibox-media-daemon" ] && continue
            count=$((count + 1))
        done &&
        test "$count" -eq 1 &&
        test ! -e /tmp/wibox-firmware-update-critical
    '
    echo "device.core PASS"
}

case "$MODE" in
    core)
        core_checks
        ;;
    media)
        core_checks
        remote 'test -c /dev/ttySGK1 && test -c /dev/gk_video'
        confirm "El timbre fisico suena y no se activa forward antes de responder por SIP"
        confirm "El snapshot del ring es una imagen real y no azul"
        confirm "Responder en el telefonillo cancela rapidamente el SIP remoto"
        confirm "Responder por SIP entrega audio PCMA bidireccional y video D1 cuando esta habilitado"
        confirm "Con video_enabled=0 la llamada audio-only no falla"
        confirm "RTSP y snapshot concurrente funcionan sin cerrar el panel ni congelar el stream"
        echo "device.media PASS"
        ;;
    ota)
        core_checks
        remote '
            test ! -e /tmp/wibox-firmware-update-critical &&
            grep -q "^WIBOX_VERSION=." /etc/wibox-release
        '
        confirm "La OTA termino, reinicio una sola vez y volvio MQTT/SSH/RTSP segun configuracion"
        echo "device.ota PASS"
        ;;
    install)
        confirm "Se creo y verifico el backup factory antes del primer flash"
        confirm "WiFi persistente o provisioning AP y acceso de recovery se validaron antes de escribir mtd4"
        confirm "La imagen transferida respeto el tamano de particion y arranco con SSH"
        core_checks
        echo "device.install PASS"
        ;;
    network)
        core_checks
        remote '
            test -x /usr/bin/wifi_mode.sh &&
            test -x /usr/bin/wifi_station_manager.sh &&
            test -x /usr/bin/wifi_portal_start.sh &&
            grep -q "^sip_port=" /mnt/mtd/sip_media.conf &&
            grep -q "^rtsp_port=" /mnt/mtd/sip_media.conf &&
            grep -q "^prometheus_port=" /mnt/mtd/sip_media.conf &&
            test ! -e /mnt/mtd/wifi_ap_requested &&
            ! pgrep hostapd >/dev/null &&
            ! pgrep httpd >/dev/null
        '
        confirm "Con el router temporalmente apagado el WiBox no reinicia ni crea AP y conecta solo cuando vuelve"
        confirm "Mantener el boton WiFi 5 segundos crea IDS7938XXXX, DHCP, web y SSH en 192.168.111.1 con LED azul intermitente"
        confirm "Guardar o cancelar cambia el LED de azul intermitente a verde antes de reiniciar"
        confirm "Cancelar el AP desde la web conserva las credenciales y devuelve el WiBox a la red guardada"
        confirm "Guardar otra red desde la web reemplaza el fichero, elimina el marcador AP y arranca en estacion"
        confirm "SSH, SIP/RTP, RTSP, Prometheus y MQTT solo son accesibles desde las redes de confianza previstas"
        echo "device.network PASS"
        ;;
    *)
        echo "Usage: $0 {core|media|ota|install|network}" >&2
        exit 2
        ;;
esac
