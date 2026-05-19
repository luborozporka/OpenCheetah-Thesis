. scripts/common.sh

OT_TESTS="hadamard_product matmul value_extension truncation exp sigmoid \
          relu argmax tanh sqrt aux_protocols maxpool"
HE_TESTS="relu maxpool argmax fc truncation"

PORT=35000

run_one () {
  local bin=$1
  build/bin/$bin r=1 p=$PORT > /tmp/$bin.alice.log 2>&1 &
  local alice_pid=$!
  sleep 0.5
  build/bin/$bin r=2 p=$PORT > /tmp/$bin.bob.log 2>&1
  local bob_rc=$?
  wait $alice_pid
  local alice_rc=$?
  if [ $alice_rc -eq 0 ] && [ $bob_rc -eq 0 ]; then
    echo -e "${GREEN}PASS${NC} $bin"
  else
    echo -e "${RED}FAIL${NC} $bin (see /tmp/$bin.*.log)"
  fi
  PORT=$((PORT+8))
}

for t in $OT_TESTS; do run_one ${t}-OT; done
for t in $HE_TESTS; do run_one ${t}-HE; done
