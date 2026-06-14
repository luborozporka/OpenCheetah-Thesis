#!/usr/bin/env bash

set -u

OUT=${1:-${SNNI_POWER_FILE:-/tmp/snni_power_uw}}
INTERVAL=${2:-${SNNI_POWER_INTERVAL:-1}}

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

probe=$(read_watts)
if [ -z "$probe" ]; then
  echo "ERROR: could not read 'Instantaneous power reading' from ipmitool dcmi." >&2
  echo "       Check: sudo -n ipmitool dcmi power reading" >&2
  exit 1
fi

TMP="${OUT}.tmp.$$"
cleanup() { rm -f "$TMP" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

echo "power-sampler: writing microwatts to '$OUT' every ${INTERVAL}s (first reading ${probe} W). Ctrl-C to stop." >&2

while true; do
  watts=$(read_watts)
  if [ -n "$watts" ]; then
    uw=$(( watts * 1000000 ))
    printf '%s\n' "$uw" > "$TMP" 2>/dev/null \
      && mv -f "$TMP" "$OUT" 2>/dev/null \
      && chmod a+r "$OUT" 2>/dev/null || true
  fi
  sleep "$INTERVAL"
done
