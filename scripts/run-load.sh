#!/usr/bin/env bash
set -u

. scripts/common.sh

usage() {
  echo "Usage: $0 [cheetah|SCI_HE] [sqnet|resnet50|densenet121] [direct:HOST[:PORT]|orchestrator:HOST[:PORT]]" >&2
  exit 1
}

[ $# -eq 3 ] || usage
backend=$1
network=$2
target=$3

contains "cheetah SCI_HE" "$backend" || usage
contains "sqnet resnet50 densenet121" "$network" || usage

case $backend in
  cheetah) binary_suffix=cheetah; routing_backend=cheetah ;;
  SCI_HE)  binary_suffix=SCI_HE;  routing_backend=sci-he ;;
esac

case $network in
  sqnet)       network_id=1; [ "$backend" = cheetah ] && bitlength=37 || bitlength=41 ;;
  resnet50)    network_id=2; bitlength=41 ;;
  densenet121) network_id=3; bitlength=41 ;;
  *) usage ;;
esac

target_kind=${target%%:*}
target_rest=${target#*:}
target_host=${target_rest%%:*}
target_port=""
case "$target_rest" in
  *:*) target_port=${target_rest#*:} ;;
esac
if [ -z "$target_host" ] || [ "$target_host" = "$target" ]; then
  echo "ERROR: target must be direct:HOST[:PORT] or orchestrator:HOST[:PORT]" >&2
  usage
fi

CONCURRENCY=${CONCURRENCY:-1}
REPS=${REPS:-1}
NUM_THREADS=${NUM_THREADS:-4}
FXP_SCALE=${FXP_SCALE:-12}
INTER_REP_SLEEP=${INTER_REP_SLEEP:-0}

timestamp=$(date +%Y%m%d-%H%M%S)
OUT_DIR=${OUT_DIR:-results/load/${network}-${backend}-c${CONCURRENCY}-${timestamp}}
mkdir -p "$OUT_DIR"

CLIENT_NODE_ID=${CLIENT_NODE_ID:-$(hostname)-client}
SNNI_OUTPUT_DIR=${SNNI_OUTPUT_DIR:-$OUT_DIR/client_measurements}
SNNI_NODE_ID=${SNNI_NODE_ID:-$CLIENT_NODE_ID}
mkdir -p "$SNNI_OUTPUT_DIR"
export SNNI_OUTPUT_DIR SNNI_NODE_ID

INPUT_PATH=${INPUT_PATH:-$(ls pretrained/${network}_input_scale12_pred*.inp 2>/dev/null | head -n 1)}
if [ -z "$INPUT_PATH" ] || [ ! -f "$INPUT_PATH" ]; then
  echo "ERROR: input not found (set INPUT_PATH=...); looked for pretrained/${network}_input_scale12_pred*.inp" >&2
  exit 1
fi
if [ -z "${EXPECTED_LABEL:-}" ]; then
  EXPECTED_LABEL=$(echo "$INPUT_PATH" | sed -n 's/.*_pred\([0-9][0-9]*\)\.inp$/\1/p')
  [ -z "$EXPECTED_LABEL" ] && EXPECTED_LABEL=-1
fi

server_ip="$target_host"
if [ "$target_kind" = direct ]; then
  [ -z "$target_port" ] && target_port=12345
  resolved=$(getent ahostsv4 "$target_host" 2>/dev/null | awk '{print $1; exit}')
  [ -n "$resolved" ] && server_ip="$resolved"
  SERVER_NODE_ID=${SERVER_NODE_ID:-$target_host}
elif [ "$target_kind" = orchestrator ]; then
  [ -z "$target_port" ] && target_port=18080
  SERVER_NODE_ID=${SERVER_NODE_ID:-}
else
  echo "ERROR: unknown target kind '$target_kind'" >&2
  usage
fi

requests_csv="$OUT_DIR/requests.csv"
windows_csv="$OUT_DIR/windows.csv"
echo "rep,client_idx,backend,network,concurrency,target_kind,node_id,server_ip,data_port,start_ms,end_ms,latency_ms,predicted_label,expected_label,label_found,success,exit_code" > "$requests_csv"
echo "rep,concurrency,start_ms,end_ms" > "$windows_csv"

cat > "$OUT_DIR/meta.env" <<EOF
backend=$backend
routing_backend=$routing_backend
network=$network
network_id=$network_id
bitlength=$bitlength
target_kind=$target_kind
target_host=$target_host
target_port=$target_port
server_node_id=$SERVER_NODE_ID
client_node_id=$CLIENT_NODE_ID
concurrency=$CONCURRENCY
reps=$REPS
num_threads=$NUM_THREADS
fxp_scale=$FXP_SCALE
input_path=$INPUT_PATH
expected_label=$EXPECTED_LABEL
snni_output_dir=$SNNI_OUTPUT_DIR
EOF

now_ms() { date +%s%3N; }

handshake_direct() {
  python3 - "$server_ip" "$target_port" "$network_id" "$bitlength" "$FXP_SCALE" "$NUM_THREADS" <<'PY'
import socket, struct, sys
ip, p, nw, bl, sc, nt = sys.argv[1:7]
try:
    s = socket.create_connection((ip, int(p)), timeout=30)
    s.sendall(b'SNNI' + struct.pack('<iiii', int(nw), int(bl), int(sc), int(nt)))
    buf = b''
    while len(buf) < 8:
        chunk = s.recv(8 - len(buf))
        if not chunk:
            break
        buf += chunk
    if len(buf) < 8:
        sys.exit('short handshake response')
    status, data_port = struct.unpack('<iI', buf)
    if status != 0:
        sys.exit('handshake status=%d' % status)
    print(data_port)
except Exception as exc:  # noqa: BLE001
    sys.stderr.write('handshake error: %s\n' % exc)
    sys.exit(1)
PY
}

run_worker() {
  local rep=$1 idx=$2 out_file=$3
  local worker_log="$out_file.log"
  local start end latency exit_code data_port node_id worker_server_ip
  local predicted expected label_found success
  expected="$EXPECTED_LABEL"
  data_port=""
  node_id=""
  worker_server_ip="$server_ip"
  predicted=""
  label_found=0
  success=0

  if [ "$target_kind" = direct ]; then
    start=$(now_ms)
    data_port=$(handshake_direct)
    if [ -z "$data_port" ]; then
      end=$(now_ms)
      exit_code=70
    else
      build/bin/${network}-${binary_suffix} \
        r=2 k=$FXP_SCALE ell=$bitlength nt=$NUM_THREADS \
        ip=$worker_server_ip p=$data_port < "$INPUT_PATH" > "$worker_log" 2>&1
      exit_code=$?
      end=$(now_ms)
      node_id="$SERVER_NODE_ID"
    fi
  else
    start=$(now_ms)
    build/bin/client \
      backend=$routing_backend \
      network=$network \
      input="$INPUT_PATH" \
      expected_label=$expected \
      orchestrator_ip="$target_host" \
      orchestrator_port=$target_port \
      binary_dir=build/bin \
      ell=$bitlength \
      k=$FXP_SCALE \
      nt=$NUM_THREADS \
      request_id="r${rep}c${idx}-$(now_ms)" > "$worker_log" 2>&1
    exit_code=$?
    end=$(now_ms)
    local summary
    summary=$(grep '^\[client\] request_id=' "$worker_log" | tail -n 1)
    if [ -n "$summary" ]; then
      node_id=$(echo "$summary" | sed -n 's/.*node_id=\([^ ]*\).*/\1/p')
      worker_server_ip=$(echo "$summary" | sed -n 's/.*server_ip=\([^ ]*\).*/\1/p')
      data_port=$(echo "$summary" | sed -n 's/.*data_port=\([0-9]*\).*/\1/p')
    fi
  fi

  predicted=$(sed -n 's/.*predicted label *= *\([0-9][0-9]*\).*/\1/p' "$worker_log" 2>/dev/null | head -n 1)
  if [ -n "$predicted" ]; then
    label_found=1
  fi

  latency=$(( end - start ))
  if [ "$exit_code" -eq 0 ] && [ "$label_found" -eq 1 ]; then
    if [ "$expected" = "-1" ] || [ -z "$expected" ] || [ "$predicted" = "$expected" ]; then
      success=1
    fi
  fi

  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$rep" "$idx" "$backend" "$network" "$CONCURRENCY" "$target_kind" \
    "$node_id" "$worker_server_ip" "$data_port" "$start" "$end" "$latency" \
    "$predicted" "$expected" "$label_found" "$success" "$exit_code" > "$out_file"
}

echo -e "Driving ${GREEN}${network}/${backend}${NC} via ${GREEN}${target_kind}${NC} ${target_host}:${target_port}  concurrency=${CONCURRENCY} reps=${REPS}"
echo -e "Output: ${GREEN}${OUT_DIR}${NC}"

total_success=0
total_requests=0
for rep in $(seq 1 "$REPS"); do
  tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/run-load.XXXXXX")
  pids=""
  for idx in $(seq 1 "$CONCURRENCY"); do
    run_worker "$rep" "$idx" "$tmp_dir/w${idx}.csv" &
    pids="$pids $!"
  done
  wait $pids

  rep_start=""
  rep_end=""
  for idx in $(seq 1 "$CONCURRENCY"); do
    line=$(cat "$tmp_dir/w${idx}.csv" 2>/dev/null)
    [ -z "$line" ] && continue
    echo "$line" >> "$requests_csv"
    total_requests=$(( total_requests + 1 ))
    s=$(echo "$line" | cut -d, -f10)
    e=$(echo "$line" | cut -d, -f11)
    suc=$(echo "$line" | cut -d, -f16)
    [ "$suc" = "1" ] && total_success=$(( total_success + 1 ))
    if [ -z "$rep_start" ] || { [ -n "$s" ] && [ "$s" -lt "$rep_start" ]; }; then rep_start=$s; fi
    if [ -z "$rep_end" ] || { [ -n "$e" ] && [ "$e" -gt "$rep_end" ]; }; then rep_end=$e; fi
  done
  echo "$rep,$CONCURRENCY,$rep_start,$rep_end" >> "$windows_csv"
  echo -e "  rep ${rep}/${REPS}: window [${rep_start}, ${rep_end}] ms"
  rm -rf "$tmp_dir"

  if [ "$rep" -lt "$REPS" ] && [ "$INTER_REP_SLEEP" != "0" ]; then
    sleep "$INTER_REP_SLEEP"
  fi
done

echo -e "${GREEN}Done.${NC} success=${total_success}/${total_requests}  requests=${requests_csv}  windows=${windows_csv}"
