#!/usr/bin/env bash

. scripts/common.sh

policy=${1:-round_robin}
if [ $# -gt 1 ] || ! contains "round_robin least_connections energy_aware" "$policy"; then
  echo "Usage: $0 [round_robin|least_connections|energy_aware]"
  exit 1
fi

ORCHESTRATOR_PORT=${ORCHESTRATOR_PORT:-18080}
HEARTBEAT_TIMEOUT_MS=${HEARTBEAT_TIMEOUT_MS:-10000}
ORCHESTRATION_LOG_DIR=${ORCHESTRATION_LOG_DIR:-results/orchestration}
MIN_MEM_AVAILABLE_BYTES=${MIN_MEM_AVAILABLE_BYTES:-0}

mkdir -p "$ORCHESTRATION_LOG_DIR"

knowledge_base_arg=""
if [ -n "$KNOWLEDGE_BASE" ]; then
  knowledge_base_arg="knowledge_base=$KNOWLEDGE_BASE"
fi

echo -e "Starting ${GREEN}build/bin/orchestrator${NC} on port $ORCHESTRATOR_PORT with policy $policy..."
build/bin/orchestrator \
  p=$ORCHESTRATOR_PORT \
  policy=$policy \
  heartbeat_timeout_ms=$HEARTBEAT_TIMEOUT_MS \
  min_mem_available_bytes=$MIN_MEM_AVAILABLE_BYTES \
  node_metrics_log="$ORCHESTRATION_LOG_DIR/node_metrics.csv" \
  routing_decisions_log="$ORCHESTRATION_LOG_DIR/routing_decisions.csv" \
  $knowledge_base_arg
