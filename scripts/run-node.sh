#!/usr/bin/env bash
set -u

. scripts/common.sh

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
  echo "Usage: $0 [server|client] <node_id> [orchestrator_host]" >&2
  exit 1
fi
role=$1
NODE_ID=$2
ORCHESTRATOR_IP=${3:-${ORCHESTRATOR_IP:-127.0.0.1}}
if [ "$role" != server ] && [ "$role" != client ]; then
  echo "role must be 'server' or 'client'" >&2
  exit 1
fi

BACKEND=${BACKEND:-cheetah}
SERVER_PORT=${SERVER_PORT:-12345}
STATUS_PORT=${STATUS_PORT:-$(( SERVER_PORT + 10000 ))}
ORCHESTRATOR_PORT=${ORCHESTRATOR_PORT:-18080}
IDLE_POWER_W=${IDLE_POWER_W:-0}
POWER_PATH=${POWER_PATH:-/tmp/snni_power_uw}
POWER_LOG=${POWER_LOG:-results/power/${NODE_ID}.csv}
POWER_INTERVAL=${POWER_INTERVAL:-1}
ALLOW_NO_POWER_SENSOR=${ALLOW_NO_POWER_SENSOR:-0}
NODE_REPORTER_INTERVAL_MS=${NODE_REPORTER_INTERVAL_MS:-1000}
SNNI_OUTPUT_DIR=${SNNI_OUTPUT_DIR:-results/measurements/${NODE_ID}}

case $BACKEND in
  cheetah) routing_backend=cheetah ;;
  SCI_HE)  routing_backend=sci-he ;;
  *) echo "BACKEND must be cheetah or SCI_HE" >&2; exit 1 ;;
esac

mkdir -p "$(dirname "$POWER_LOG")" "$SNNI_OUTPUT_DIR" results/orchestration

export SNNI_POWER_PATH="$POWER_PATH"
export SNNI_OUTPUT_DIR
export SNNI_NODE_ID="$NODE_ID"

sampler_pid=""
server_pid=""
reporter_pid=""
cleanup() {
  for pid in "$reporter_pid" "$server_pid" "$sampler_pid"; do
    [ -n "$pid" ] && kill "$pid" 2>/dev/null || true
  done
}
trap cleanup EXIT INT TERM

echo -e "Starting ${GREEN}power-sampler${NC} (latest=$POWER_PATH, log=$POWER_LOG, ${POWER_INTERVAL}s)..."
scripts/power-sampler.sh "$POWER_PATH" "$POWER_INTERVAL" "$POWER_LOG" &
sampler_pid=$!
sleep 2

if [ "$role" = server ]; then
  SQNET_WEIGHTS=${SQNET_WEIGHTS:-pretrained/sqnet_model_scale12.inp}
  RESNET50_WEIGHTS=${RESNET50_WEIGHTS:-pretrained/resnet50_model_scale12.inp}
  DENSENET121_WEIGHTS=${DENSENET121_WEIGHTS:-pretrained/densenet121_model_scale12.inp}

  weights_args=""
  [ -f "$SQNET_WEIGHTS" ] && weights_args="$weights_args sqnet_weights=$SQNET_WEIGHTS"
  [ -f "$RESNET50_WEIGHTS" ] && weights_args="$weights_args resnet50_weights=$RESNET50_WEIGHTS"
  [ -f "$DENSENET121_WEIGHTS" ] && weights_args="$weights_args densenet121_weights=$DENSENET121_WEIGHTS"
  if [ -z "$weights_args" ]; then
    echo "ERROR: no weights found; set SQNET_WEIGHTS/RESNET50_WEIGHTS/DENSENET121_WEIGHTS" >&2
    exit 1
  fi

  echo -e "Starting ${GREEN}build/bin/server-${BACKEND}${NC} (control=$SERVER_PORT status=$STATUS_PORT)..."
  build/bin/server-$BACKEND p=$SERVER_PORT sp=$STATUS_PORT $weights_args &
  server_pid=$!
  sleep 2

  echo -e "Starting ${GREEN}node-reporter${NC} (role=server, node=$NODE_ID -> $ORCHESTRATOR_IP:$ORCHESTRATOR_PORT)..."
  build/bin/node-reporter \
    node_id="$NODE_ID" \
    node_role=server \
    backend=$routing_backend \
    server_ip=127.0.0.1 \
    control_port=$SERVER_PORT \
    status_port=$STATUS_PORT \
    idle_power_w=$IDLE_POWER_W \
    power_path="$POWER_PATH" \
    allow_no_power_sensor=$ALLOW_NO_POWER_SENSOR \
    orchestrator_ip="$ORCHESTRATOR_IP" \
    orchestrator_port=$ORCHESTRATOR_PORT \
    interval_ms=$NODE_REPORTER_INTERVAL_MS &
  reporter_pid=$!

  echo -e "${GREEN}Server node '$NODE_ID' up.${NC} Ctrl-C to stop."
  wait "$server_pid"
else
  echo -e "Starting ${GREEN}node-reporter${NC} (role=client, node=$NODE_ID -> $ORCHESTRATOR_IP:$ORCHESTRATOR_PORT)..."
  build/bin/node-reporter \
    node_id="$NODE_ID" \
    node_role=client \
    idle_power_w=$IDLE_POWER_W \
    power_path="$POWER_PATH" \
    allow_no_power_sensor=$ALLOW_NO_POWER_SENSOR \
    orchestrator_ip="$ORCHESTRATOR_IP" \
    orchestrator_port=$ORCHESTRATOR_PORT \
    interval_ms=$NODE_REPORTER_INTERVAL_MS &
  reporter_pid=$!

  echo -e "${GREEN}Client node '$NODE_ID' up.${NC} Drive load with scripts/run-load.sh. Ctrl-C to stop."
  wait "$reporter_pid"
fi
