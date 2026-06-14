#!/usr/bin/env bash

. scripts/common.sh

if [ $# -lt 1 ] || [ $# -gt 2 ] || ! contains "cheetah SCI_HE" "$1"; then
  echo "Usage: $0 [cheetah|SCI_HE] [server_ip]"
  exit 1
fi

backend=$1
server_ip=$2
if [ -z "$server_ip" ]; then
  server_ip=$(hostname -I | awk '{print $1}')
fi
if [ -z "$server_ip" ]; then
  echo "Could not determine server IP; pass it explicitly as the second argument."
  exit 1
fi

case $backend in
  cheetah) reporter_backend=cheetah ;;
  SCI_HE) reporter_backend=sci-he ;;
esac

ORCHESTRATOR_IP=${ORCHESTRATOR_IP:-127.0.0.1}
ORCHESTRATOR_PORT=${ORCHESTRATOR_PORT:-18080}
STATUS_PORT=${STATUS_PORT:-$((SERVER_PORT + 10000))}
MAX_SESSIONS=${MAX_SESSIONS:-$NUM_THREADS}
NODE_REPORTER_INTERVAL_MS=${NODE_REPORTER_INTERVAL_MS:-1000}
ALLOW_NO_POWER_SENSOR=${ALLOW_NO_POWER_SENSOR:-1}
IDLE_POWER_W=${IDLE_POWER_W:-0}
NODE_ID=${NODE_ID:-$(hostname)-$backend}
POWER_PATH=${POWER_PATH:-}
SNNI_POWER_PATH=${SNNI_POWER_PATH:-$POWER_PATH}
SNNI_OUTPUT_DIR=${SNNI_OUTPUT_DIR:-results/measurements}
SNNI_NODE_ID=${SNNI_NODE_ID:-$NODE_ID}

cleanup() {
  if [ -n "$server_pid" ]; then
    kill "$server_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

mkdir -p "$SNNI_OUTPUT_DIR"
export SNNI_OUTPUT_DIR SNNI_NODE_ID
if [ -n "$SNNI_POWER_PATH" ]; then export SNNI_POWER_PATH; fi

echo -e "Starting ${GREEN}build/bin/server-$backend${NC} on control port $SERVER_PORT..."
build/bin/server-$backend \
  p=$SERVER_PORT \
  sp=$STATUS_PORT \
  max_sessions=$MAX_SESSIONS \
  sqnet_weights=pretrained/sqnet_model_scale12.inp \
  resnet50_weights=pretrained/resnet50_model_scale12.inp \
  densenet121_weights=pretrained/densenet121_model_scale12.inp &
server_pid=$!

sleep 1

echo -e "Starting ${GREEN}build/bin/node-reporter${NC} for $NODE_ID..."
build/bin/node-reporter \
  node_id="$NODE_ID" \
  node_role=server \
  server_ip="$server_ip" \
  backend=$reporter_backend \
  control_port=$SERVER_PORT \
  status_port=$STATUS_PORT \
  idle_power_w=$IDLE_POWER_W \
  power_path="$POWER_PATH" \
  allow_no_power_sensor=$ALLOW_NO_POWER_SENSOR \
  orchestrator_ip="$ORCHESTRATOR_IP" \
  orchestrator_port=$ORCHESTRATOR_PORT \
  interval_ms=$NODE_REPORTER_INTERVAL_MS
