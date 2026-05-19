. scripts/common.sh

if [ $# -lt 2 ] || [ $# -gt 3 ] || ! contains "cheetah SCI_HE" $1; then
  echo "Usage: $0 [cheetah|SCI_HE] [sqnet|resnet50|densenet121] [server_ip]"
  exit 1
fi
backend=$1
network=$2
if [ -n "$3" ]; then
  SERVER_IP=$(getent ahostsv4 "$3" | awk '{print $1; exit}')
  [ -n "$SERVER_IP" ] || { echo "Could not resolve $3 to an IPv4 address"; exit 1; }
fi

case $network in
  sqnet)       network_id=1; [ "$backend" = cheetah ] && bitlength=37 || bitlength=41 ;;
  resnet50)    network_id=2; bitlength=41 ;;
  densenet121) network_id=3; bitlength=41 ;;
  *) echo "Usage: $0 [cheetah|SCI_HE] [sqnet|resnet50|densenet121] [server_ip]"; exit 1 ;;
esac

data_port=$(python3 - "$SERVER_IP" "$SERVER_PORT" "$network_id" "$bitlength" "$FXP_SCALE" "$NUM_THREADS" <<'PY'
import socket, struct, sys
ip, p, nw, bl, sc, nt = sys.argv[1:7]
s = socket.create_connection((ip, int(p)))
s.sendall(b'SNNI' + struct.pack('<iiii', int(nw), int(bl), int(sc), int(nt)))
status, data_port = struct.unpack('<iI', s.recv(8))
if status != 0:
    sys.exit(f'handshake error: status={status}')
print(data_port)
PY
)
[ -z "$data_port" ] && exit 1

echo -e "Got data port ${GREEN}$data_port${NC} from $SERVER_IP; running ${GREEN}${network}-${backend}${NC} client..."
cat pretrained/${network}_input_scale12_pred*.inp | \
  build/bin/${network}-$backend r=2 k=$FXP_SCALE ell=$bitlength nt=$NUM_THREADS ip=$SERVER_IP p=$data_port
