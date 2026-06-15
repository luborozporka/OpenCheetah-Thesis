#!/usr/bin/env bash
set -u

. scripts/common.sh

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
  echo "Usage: $0 <server_host> <server_node_id> [server_port]" >&2
  exit 1
fi

SERVER_HOST=$1
SERVER_NODE_ID=$2
SERVER_PORT=${3:-12345}
STATUS_PORT=$(( SERVER_PORT + 10000 ))

BACKENDS=${BACKENDS:-"cheetah SCI_HE"}
NETWORKS=${NETWORKS:-"sqnet resnet50 densenet121"}
CONCURRENCIES=${CONCURRENCIES:-"1 2 4"}
REPS=${REPS:-30}
OUT_ROOT=${OUT_ROOT:-results/kb}
INTER_REP_SLEEP=${INTER_REP_SLEEP:-3}
NUM_THREADS=${NUM_THREADS:-4}
FXP_SCALE=${FXP_SCALE:-12}
CELL_COOLDOWN_SECONDS=${CELL_COOLDOWN_SECONDS:-60}

MANAGE_SERVER=${MANAGE_SERVER:-1}
REMOTE_REPO=${REMOTE_REPO:-$(pwd)}
SERVER_POWER_PATH=${SERVER_POWER_PATH:-/tmp/snni_power_uw}
SERVER_STARTUP_TIMEOUT=${SERVER_STARTUP_TIMEOUT:-60}
PIDFILE=${PIDFILE:-/tmp/snni_kb_server_${SERVER_PORT}.pid}

SQNET_WEIGHTS=${SQNET_WEIGHTS:-pretrained/sqnet_model_scale12.inp}
RESNET50_WEIGHTS=${RESNET50_WEIGHTS:-pretrained/resnet50_model_scale12.inp}
DENSENET121_WEIGHTS=${DENSENET121_WEIGHTS:-pretrained/densenet121_model_scale12.inp}

export REPS INTER_REP_SLEEP NUM_THREADS FXP_SCALE SERVER_NODE_ID

is_local_host() {
  case "$1" in
    127.0.0.1|localhost|::1) return 0 ;;
  esac
  [ "$1" = "$(hostname 2>/dev/null)" ] && return 0
  [ "$1" = "$(hostname -s 2>/dev/null)" ] && return 0
  return 1
}

if [ -z "${SERVER_SSH+x}" ]; then
  if is_local_host "$SERVER_HOST"; then SERVER_SSH=""; else SERVER_SSH="ssh $SERVER_HOST"; fi
fi

runremote() {
  if [ -z "$SERVER_SSH" ]; then
    bash -c "$1"
  else
    $SERVER_SSH "$1"
  fi
}

wait_for_port() {
  local host=$1 port=$2 timeout=${3:-60}
  python3 - "$host" "$port" "$timeout" <<'PY'
import socket, sys, time
host, port, timeout = sys.argv[1], int(sys.argv[2]), float(sys.argv[3])
deadline = time.time() + timeout
while time.time() < deadline:
    try:
        socket.create_connection((host, port), timeout=2).close()
        sys.exit(0)
    except OSError:
        time.sleep(0.5)
sys.exit(1)
PY
}

weights_for_network() {
  case "$1" in
    sqnet)       echo "sqnet_weights=$SQNET_WEIGHTS" ;;
    resnet50)    echo "resnet50_weights=$RESNET50_WEIGHTS" ;;
    densenet121) echo "densenet121_weights=$DENSENET121_WEIGHTS" ;;
    *)           echo "" ;;
  esac
}

stop_server() {
  [ "$MANAGE_SERVER" = 1 ] || return 0
  runremote "if [ -f \"$PIDFILE\" ]; then kill \$(cat \"$PIDFILE\") 2>/dev/null || true; rm -f \"$PIDFILE\"; fi; command -v fuser >/dev/null 2>&1 && fuser -k ${SERVER_PORT}/tcp 2>/dev/null || true" >/dev/null 2>&1 || true
  sleep 1
}

start_server() {
  local backend=$1 network=$2 concurrency=$3
  local weights logdir logf snni_out rc
  weights=$(weights_for_network "$network")
  if [ -z "$weights" ]; then
    echo "ERROR: no weights mapping for network '$network'" >&2
    return 1
  fi
  logdir="$REMOTE_REPO/results/kb/_server_logs"
  logf="$logdir/${backend}-${network}-c${concurrency}.log"
  snni_out="$REMOTE_REPO/results/kb/_server_measurements/${backend}/${network}/c${concurrency}"

  echo -e "  ${GREEN}start${NC} build/bin/server-${backend} (${network}, c${concurrency}) on ${SERVER_HOST}:${SERVER_PORT}"
  rc="cd \"$REMOTE_REPO\" && mkdir -p \"$logdir\" \"$snni_out\" && \
SNNI_NODE_ID=\"$SERVER_NODE_ID\" SNNI_OUTPUT_DIR=\"$snni_out\" SNNI_POWER_PATH=\"$SERVER_POWER_PATH\" \
nohup build/bin/server-$backend p=$SERVER_PORT sp=$STATUS_PORT $weights < /dev/null >> \"$logf\" 2>&1 & echo \$! > \"$PIDFILE\""
  runremote "$rc" >/dev/null 2>&1 || true

  if ! wait_for_port "$SERVER_HOST" "$SERVER_PORT" "$SERVER_STARTUP_TIMEOUT"; then
    echo "ERROR: server-$backend did not open ${SERVER_HOST}:${SERVER_PORT} within ${SERVER_STARTUP_TIMEOUT}s" >&2
    echo "       check the server log: $logf (on ${SERVER_SSH:-this host})" >&2
    return 1
  fi
  sleep 1
}

if [ "$MANAGE_SERVER" = 1 ]; then
  trap 'stop_server' EXIT INT TERM
fi

echo -e "${GREEN}KB profiling${NC} -> server ${SERVER_NODE_ID} (${SERVER_HOST}:${SERVER_PORT})"
echo -e "  backends=[${BACKENDS}] networks=[${NETWORKS}] concurrency=[${CONCURRENCIES}] reps=${REPS}"
echo -e "  cooldown after each cell: ${GREEN}${CELL_COOLDOWN_SECONDS}s${NC}"
echo -e "  output root: ${GREEN}${OUT_ROOT}${NC}"
if [ "$MANAGE_SERVER" = 1 ]; then
  echo -e "  server lifecycle: ${GREEN}managed${NC} (fresh process per backend x network x concurrency) via ${SERVER_SSH:-local}"
else
  echo -e "  server lifecycle: ${RED}not managed${NC} - assuming an already-running server."
  echo -e "  ${RED}NOTE:${NC} a single server serves ONE backend only; run one backend per invocation."
fi

for backend in $BACKENDS; do
  if ! contains "cheetah SCI_HE" "$backend"; then
    echo "WARN: unknown backend '$backend'; skipping" >&2
    continue
  fi
  for network in $NETWORKS; do
    echo -e "\n=== ${GREEN}${backend}/${network}${NC} ==="

    for c in $CONCURRENCIES; do
      cell_dir="$OUT_ROOT/$backend/$network/c$c"
      echo -e "--- ${GREEN}c${c}${NC} ---"

      if [ "$MANAGE_SERVER" = 1 ]; then
        stop_server
        if ! start_server "$backend" "$network" "$c"; then
          echo "WARN: skipping ${backend}/${network}/c${c} (server failed to start)" >&2
          stop_server
          [ "$CELL_COOLDOWN_SECONDS" -gt 0 ] && sleep "$CELL_COOLDOWN_SECONDS"
          continue
        fi
      fi

      OUT_DIR="$cell_dir" CONCURRENCY="$c" \
        bash scripts/run-load.sh "$backend" "$network" "direct:${SERVER_HOST}:${SERVER_PORT}" \
        || echo "WARN: cell ${backend}/${network}/c${c} failed; continuing" >&2

      [ "$MANAGE_SERVER" = 1 ] && stop_server
      if [ "$CELL_COOLDOWN_SECONDS" -gt 0 ]; then
        echo -e "  ${GREEN}cooldown${NC} ${CELL_COOLDOWN_SECONDS}s"
        sleep "$CELL_COOLDOWN_SECONDS"
      fi
    done
  done
done

echo -e "\n${GREEN}Profiling complete.${NC} Build the KB with:"
echo "  scripts/build-kb.py --root $OUT_ROOT --nodes nodes.csv --out knowledge_base.csv"
