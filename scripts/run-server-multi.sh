. scripts/common.sh

if [ ! $# -eq 1 ] || ! contains "cheetah SCI_HE" $1; then
  echo "Usage: $0 [cheetah|SCI_HE]"
  exit 1
fi

echo -e "Starting ${GREEN}build/bin/server-$1${NC} on control port $SERVER_PORT..."
build/bin/server-$1 p=$SERVER_PORT \
  sqnet_weights=pretrained/sqnet_model_scale12.inp \
  resnet50_weights=pretrained/resnet50_model_scale12.inp \
  densenet121_weights=pretrained/densenet121_model_scale12.inp
