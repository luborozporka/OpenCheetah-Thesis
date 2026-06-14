#!/usr/bin/env bash

set -u

DURATION=${1:-600}
INTERVAL=${2:-1}

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

echo "Sampling idle whole-node power for ${DURATION}s (interval ${INTERVAL}s)..." >&2
end=$(( $(date +%s) + DURATION ))
sum=0
n=0
min=""
max=""

while [ "$(date +%s)" -lt "$end" ]; do
  w=$(read_watts)
  if [ -n "$w" ]; then
    sum=$(( sum + w ))
    n=$(( n + 1 ))
    if [ -z "$min" ] || [ "$w" -lt "$min" ]; then min=$w; fi
    if [ -z "$max" ] || [ "$w" -gt "$max" ]; then max=$w; fi
  fi
  sleep "$INTERVAL"
done

if [ "$n" -eq 0 ]; then
  echo "ERROR: captured no readings; check 'sudo -n ipmitool dcmi power reading'." >&2
  exit 1
fi

mean=$(awk "BEGIN { printf \"%.2f\", $sum / $n }")
echo "node=$(hostname) idle_power_w_mean=${mean} min_w=${min} max_w=${max} samples=${n}"
