. scripts/common.sh

export CMAKE_POLICY_VERSION_MINIMUM=3.5

for deps in eigen3 emp-ot emp-tool hexl SEAL-3.7
do
  if [ ! -d $BUILD_DIR/include/$deps ] 
  then
	echo -e "${RED}$deps${NC} seems absent in ${BUILD_DIR}/include/, please re-run scripts/build-deps.sh"
	exit 1
  fi
done

for deps in zstd.h 
do
  if [ ! -f $BUILD_DIR/include/$deps ] 
  then
	echo -e "${RED}$deps${NC} seems absent in ${BUILD_DIR}/include/, please re-run scripts/build-deps.sh"
	exit 1
  fi
done

cd $BUILD_DIR/
cmake .. -DCMAKE_BUILD_TYPE=Release -DSCI_BUILD_NETWORKS=ON -DSCI_BUILD_TESTS=ON -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl -DCMAKE_PREFIX_PATH=$BUILD_DIR -DUSE_APPROX_RESHARE=ON
# for net in resnet50 sqnet densenet121
for net in resnet50 sqnet densenet121 short1 short2
do
     make ${net}-cheetah -j4 
     make ${net}-SCI_HE -j4 
done

for net in relu1_28_28_1 relu784 relu12_23_34_45 relu422280 relu1 mp1 mp2 mp3 mp4
do
     make ${net}-cheetah -j4 
done