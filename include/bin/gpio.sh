#!/bin/sh

GPIOD=${WIFI_LED_GPIO_ROOT:-/sys/class/gpio}
WIFI_LED_BLINK_PID=${WIFI_LED_BLINK_PID_PATH:-/var/run/wifi_led_blink.pid}
WIFI_LED_BLINK_INTERVAL=${WIFI_LED_BLINK_INTERVAL:-1}

setup_gpio(){
  N=$1
  V=$2
  GPION=${GPIOD}/gpio${N}

  if [ ! -e "${GPION}" ]; then
    echo ${N} > ${GPIOD}/export
  fi
  
  DIRECTION=low
  [ "$V" == "1" ] && DIRECTION=high

  echo 0 > ${GPION}/active_low
  echo $DIRECTION > ${GPION}/direction
  echo $V > ${GPION}/value
}

setup_all_gpio(){
  setup_gpio 10 0
  setup_gpio 11 0
  setup_gpio 12 0
  setup_gpio 18 1
  setup_gpio 19 0
  setup_gpio 34 0
}

set_gpio(){ echo $2 > ${GPIOD}/gpio$1/value; }

wifi_led() {
  L=
  [ "$1" == "red" ] && L=10
  [ "$1" == "green" ] && L=12
  [ "$1" == "blue" ] && L=11

  for LED in 10 11 12; do
    V=0
    [ "$LED" == "$L" ] && V=1
    set_gpio $LED $V
  done
}

wifi_led_blink_stop() {
  if [ -f "${WIFI_LED_BLINK_PID}" ]; then
    LED_PID=$(awk 'NR == 1 { print $1 }' "${WIFI_LED_BLINK_PID}" 2>/dev/null)
    LED_START=$(awk 'NR == 1 { print $2 }' "${WIFI_LED_BLINK_PID}" 2>/dev/null)
    LED_CURRENT_START=
    if [ -n "${LED_PID}" ] && [ -r "/proc/${LED_PID}/stat" ]; then
      LED_CURRENT_START=$(awk '{ print $22 }' "/proc/${LED_PID}/stat" 2>/dev/null)
    fi
    if [ -n "${LED_START}" ] && [ "${LED_CURRENT_START}" = "${LED_START}" ]; then
      kill "${LED_PID}" 2>/dev/null || true
    fi
    rm -f "${WIFI_LED_BLINK_PID}"
  fi
}

wifi_led_blink_start() {
  LED_COLOR=$1
  wifi_led_blink_stop
  (
    trap 'exit 0' HUP INT TERM
    while :; do
      wifi_led "${LED_COLOR}"
      sleep "${WIFI_LED_BLINK_INTERVAL}"
      wifi_led off
      sleep "${WIFI_LED_BLINK_INTERVAL}"
    done
  ) &
  LED_PID=$!
  LED_START=$(awk '{ print $22 }' "/proc/${LED_PID}/stat" 2>/dev/null)
  printf '%s %s\n' "${LED_PID}" "${LED_START}" > "${WIFI_LED_BLINK_PID}"
}
