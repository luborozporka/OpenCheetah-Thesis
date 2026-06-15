#!/usr/bin/env bash

set -u

OUT=${1:-${SNNI_POWER_FILE:-/tmp/snni_power_uw}}
INTERVAL=${2:-${SNNI_POWER_INTERVAL:-1}}
POWER_LOG=${3:-${SNNI_POWER_LOG:-}}

if ! command -v ipmitool >/dev/null 2>&1; then
  echo "ERROR: ipmitool not found. Install it (sudo apt-get install -y ipmitool)." >&2
  exit 1
fi

if [ "$(id -u)" -eq 0 ]; then
  IPMI="ipmitool"
else
  IPMI="sudo -n ipmitool"
fi

read_watts() {
  $IPMI dcmi power reading 2>/dev/null \
    | sed -n 's/.*Instantaneous power reading[: ]*\([0-9][0-9]*\).*/\1/p' \
    | head -n 1
}

now_ms() { date +%s%3N; }

probe=$(read_watts)
if [ -z "$probe" ]; then
  echo "ERROR: could not read 'Instantaneous power reading' from ipmitool dcmi." >&2
  echo "       Check: sudo -n ipmitool dcmi power reading" >&2
  exit 1
fi

TMP="${OUT}.tmp.$$"
cleanup() { rm -f "$TMP" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

if [ -n "$POWER_LOG" ]; then
  mkdir -p "$(dirname "$POWER_LOG")" 2>/dev/null || true
  [ -s "$POWER_LOG" ] || printf 'timestamp_ms,power_uw\n' >> "$POWER_LOG"
  echo "power-sampler: latest -> '$OUT', history -> '$POWER_LOG', every ${INTERVAL}s (first ${probe} W). Ctrl-C to stop." >&2
else
  echo "power-sampler: writing microwatts to '$OUT' every ${INTERVAL}s (first reading ${probe} W). Ctrl-C to stop." >&2
fi

while true; do
  watts=$(read_watts)
  if [ -n "$watts" ]; then
    uw=$(( watts * 1000000 ))
    printf '%s\n' "$uw" > "$TMP" 2>/dev/null \
      && mv -f "$TMP" "$OUT" 2>/dev/null \
      && chmod a+r "$OUT" 2>/dev/null || true
    if [ -n "$POWER_LOG" ]; then
      printf '%s,%s\n' "$(now_ms)" "$uw" >> "$POWER_LOG" 2>/dev/null || true
    fi
  fi
  sleep "$INTERVAL"
done
