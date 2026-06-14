#!/usr/bin/env bash
set -u

. scripts/common.sh

if [ $# -ne 2 ]; then
  echo "Usage: $0 [cheetah|SCI_HE] [sqnet|resnet50|densenet121]" >&2
  exit 1
fi
backend=$1
network=$2
contains "cheetah SCI_HE" "$backend" || { echo "bad backend" >&2; exit 1; }
contains "sqnet resnet50 densenet121" "$network" || { echo "bad network" >&2; exit 1; }

POLICIES=${POLICIES:-"round_robin least_connections energy_aware"}
KNOWLEDGE_BASE=${KNOWLEDGE_BASE:-knowledge_base.csv}
ORCHESTRATOR_PORT=${ORCHESTRATOR_PORT:-18080}
HEARTBEAT_TIMEOUT_MS=${HEARTBEAT_TIMEOUT_MS:-10000}
MIN_MEM_AVAILABLE_BYTES=${MIN_MEM_AVAILABLE_BYTES:-0}
CONCURRENCY=${CONCURRENCY:-4}
REPS=${REPS:-20}
WARMUP_SECONDS=${WARMUP_SECONDS:-8}
timestamp=$(date +%Y%m%d-%H%M%S)
OUT_ROOT=${OUT_ROOT:-results/policy/${backend}-${network}-${timestamp}}

if [ ! -f "$KNOWLEDGE_BASE" ]; then
  echo "WARN: knowledge_base '$KNOWLEDGE_BASE' not found; energy_aware will fall back to defaults." >&2
fi

mkdir -p "$OUT_ROOT"
orchestrator_pid=""
cleanup() {
  if [ -n "$orchestrator_pid" ]; then
    kill "$orchestrator_pid" 2>/dev/null || true
    wait "$orchestrator_pid" 2>/dev/null || true
    orchestrator_pid=""
  fi
}
trap cleanup EXIT INT TERM

for policy in $POLICIES; do
  contains "round_robin least_connections energy_aware" "$policy" || { echo "bad policy $policy" >&2; continue; }
  policy_dir="$OUT_ROOT/$policy"
  load_dir="$policy_dir/load"
  mkdir -p "$policy_dir"
  echo -e "\n=== ${GREEN}policy=${policy}${NC} ==="

  kb_arg=""
  [ -f "$KNOWLEDGE_BASE" ] && kb_arg="knowledge_base=$KNOWLEDGE_BASE"

  echo -e "Starting ${GREEN}orchestrator${NC} (policy=$policy) on port $ORCHESTRATOR_PORT..."
  build/bin/orchestrator \
    p=$ORCHESTRATOR_PORT \
    policy=$policy \
    heartbeat_timeout_ms=$HEARTBEAT_TIMEOUT_MS \
    min_mem_available_bytes=$MIN_MEM_AVAILABLE_BYTES \
    node_metrics_log="$policy_dir/node_metrics.csv" \
    routing_decisions_log="$policy_dir/routing_decisions.csv" \
    $kb_arg &
  orchestrator_pid=$!

  echo "Waiting ${WARMUP_SECONDS}s for nodes to register..."
  sleep "$WARMUP_SECONDS"
  if ! kill -0 "$orchestrator_pid" 2>/dev/null; then
    echo "ERROR: orchestrator exited during warmup (port in use?); skipping policy $policy" >&2
    orchestrator_pid=""
    continue
  fi

  if [ -n "${WORKLOAD_CMD:-}" ]; then
    OUT_DIR="$load_dir" CONCURRENCY="$CONCURRENCY" REPS="$REPS" \
      bash -c "$WORKLOAD_CMD" \
      || echo "WARN: workload failed for policy $policy" >&2
  else
    OUT_DIR="$load_dir" CONCURRENCY="$CONCURRENCY" REPS="$REPS" \
      scripts/run-load.sh "$backend" "$network" "orchestrator:127.0.0.1:${ORCHESTRATOR_PORT}" \
      || echo "WARN: workload failed for policy $policy" >&2
  fi

  echo "Stopping orchestrator for policy $policy..."
  cleanup
  sleep 1
done

echo -e "\n${GREEN}Policy experiment complete.${NC} Results under ${OUT_ROOT}/<policy>/"
