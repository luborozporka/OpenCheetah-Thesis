#!/usr/bin/env bash
set -u

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
  echo "Usage: $0 <pid> <mem_log.csv> [interval_seconds]" >&2
  exit 1
fi

PID=$1
MEM_LOG=$2
INTERVAL=${3:-0.5}

if [ ! -d "/proc/$PID" ]; then
  echo "ERROR: process $PID not found" >&2
  exit 1
fi

mkdir -p "$(dirname "$MEM_LOG")" 2>/dev/null || true
[ -s "$MEM_LOG" ] || printf 'timestamp_ms,pid,rss_kb,hwm_kb,mem_used_kb,mem_available_kb\n' > "$MEM_LOG"

now_ms() { date +%s%3N; }

read_proc_mem_kb() {
  awk '
    /^VmRSS:/ { rss=$2 }
    /^VmHWM:/ { hwm=$2 }
    END { printf "%s %s\n", rss + 0, hwm + 0 }
  ' "/proc/$PID/status" 2>/dev/null
}

read_system_mem_kb() {
  awk '
    /^MemTotal:/ { total=$2 }
    /^MemAvailable:/ { available=$2 }
    END { printf "%s %s\n", total - available, available + 0 }
  ' /proc/meminfo 2>/dev/null
}

while kill -0 "$PID" 2>/dev/null; do
  proc_mem=$(read_proc_mem_kb)
  system_mem=$(read_system_mem_kb)
  if [ -n "$proc_mem" ] && [ -n "$system_mem" ]; then
    printf '%s,%s,%s,%s,%s,%s\n' "$(now_ms)" "$PID" $proc_mem $system_mem >> "$MEM_LOG"
  fi
  sleep "$INTERVAL"
done
