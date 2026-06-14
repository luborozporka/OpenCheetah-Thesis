#!/usr/bin/env bash

. scripts/common.sh

if [ $# -lt 2 ] || [ $# -gt 3 ] || ! contains "cheetah SCI_HE" "$1"; then
  echo "Usage: $0 [cheetah|SCI_HE] [sqnet|resnet50|densenet121] [orchestrator_ip]"
  exit 1
fi

backend=$1
network=$2
ORCHESTRATOR_IP=${3:-${ORCHESTRATOR_IP:-127.0.0.1}}
ORCHESTRATOR_PORT=${ORCHESTRATOR_PORT:-18080}
ORCHESTRATION_LOG_DIR=${ORCHESTRATION_LOG_DIR:-results/orchestration}
NODE_REPORTER_INTERVAL_MS=${NODE_REPORTER_INTERVAL_MS:-1000}
ALLOW_NO_POWER_SENSOR=${ALLOW_NO_POWER_SENSOR:-1}
IDLE_POWER_W=${IDLE_POWER_W:-0}
RUN_CLIENT_REPORTER=${RUN_CLIENT_REPORTER:-1}
NODE_ID=${NODE_ID:-$(hostname)-client}
POWER_PATH=${POWER_PATH:-}
SNNI_POWER_PATH=${SNNI_POWER_PATH:-$POWER_PATH}
SNNI_OUTPUT_DIR=${SNNI_OUTPUT_DIR:-results/measurements}
SNNI_NODE_ID=${SNNI_NODE_ID:-$NODE_ID}

case $backend in
  cheetah) routing_backend=cheetah ;;
  SCI_HE) routing_backend=sci-he ;;
esac

case $network in
  sqnet)
    if [ "$backend" = cheetah ]; then
      bitlength=37
    else
      bitlength=41
    fi
    ;;
  resnet50|densenet121)
    bitlength=41
    ;;
  *)
    echo "Usage: $0 [cheetah|SCI_HE] [sqnet|resnet50|densenet121] [orchestrator_ip]"
    exit 1
    ;;
esac

input_path=$(ls pretrained/${network}_input_scale12_pred*.inp 2>/dev/null | head -n 1)
if [ -z "$input_path" ]; then
  echo "Could not find pretrained/${network}_input_scale12_pred*.inp"
  exit 1
fi

expected_label=$(echo "$input_path" | sed -n 's/.*_pred\([0-9][0-9]*\)\.inp$/\1/p')
mkdir -p "$ORCHESTRATION_LOG_DIR"

cleanup() {
  if [ -n "$reporter_pid" ]; then
    kill "$reporter_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

if [ "$RUN_CLIENT_REPORTER" != "0" ]; then
  echo -e "Starting ${GREEN}build/bin/node-reporter${NC} for $NODE_ID..."
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
  sleep 1
fi

mkdir -p "$SNNI_OUTPUT_DIR"
export SNNI_OUTPUT_DIR SNNI_NODE_ID
if [ -n "$SNNI_POWER_PATH" ]; then export SNNI_POWER_PATH; fi

echo -e "Running ${GREEN}build/bin/client${NC} through orchestrator $ORCHESTRATOR_IP:$ORCHESTRATOR_PORT..."
build/bin/client \
  backend=$routing_backend \
  network=$network \
  input="$input_path" \
  expected_label=$expected_label \
  orchestrator_ip="$ORCHESTRATOR_IP" \
  orchestrator_port=$ORCHESTRATOR_PORT \
  binary_dir=build/bin \
  ell=$bitlength \
  k=$FXP_SCALE \
  nt=$NUM_THREADS \
  request_results_log="$ORCHESTRATION_LOG_DIR/request_results.csv"
