#ifndef SCI_SESSION_H___
#define SCI_SESSION_H___

#include "NonLinear/argmax.h"
#include "NonLinear/maxpool.h"
#include "NonLinear/relu-interface.h"
#include "OT/kkot.h"
#include "csv_writer.hpp"
#include "defines.h"
#include "defines_uniform.h"

#ifdef SCI_OT
#include "BuildingBlocks/aux-protocols.h"
#include "BuildingBlocks/truncation.h"
#include "BuildingBlocks/value-extension.h"
#include "LinearOT/linear-ot.h"
#include "LinearOT/linear-uniform.h"
#include "Math/math-functions.h"
#endif

#ifdef SCI_HE
#include "LinearHE/conv-field.h"
#include "LinearHE/elemwise-prod-field.h"
#include "LinearHE/fc-field.h"
#endif

#if USE_CHEETAH
#include "cheetah/cheetah-api.h"
#endif

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#ifndef MAX_THREADS
#define MAX_THREADS 4
#endif

namespace sci {

struct Config {
  int party = 0;
  std::string address = "127.0.0.1";
  int port = 0;
  int num_threads = 1;
  int32_t bitlength = 32;
  uint64_t prime_mod = 0;
};

struct IOContext {
  sci::NetIO *primary = nullptr;
  sci::OTPack<sci::NetIO> *otpack = nullptr;
  sci::IKNP<sci::NetIO> *iknpOT = nullptr;
  sci::IKNP<sci::NetIO> *iknpOTRoleReversed = nullptr;
  sci::KKOT<sci::NetIO> *kkot = nullptr;
  sci::PRG128 *prg128 = nullptr;
  sci::NetIO *ioArr[MAX_THREADS] = {};
  sci::OTPack<sci::NetIO> *otpackArr[MAX_THREADS] = {};
  sci::IKNP<sci::NetIO> *otInstanceArr[MAX_THREADS] = {};
  sci::KKOT<sci::NetIO> *kkotInstanceArr[MAX_THREADS] = {};
  sci::PRG128 *prgInstanceArr[MAX_THREADS] = {};
};

struct NonLinearContext {
  ReLUProtocol<sci::NetIO, intType> *relu = nullptr;
  MaxPoolProtocol<sci::NetIO, intType> *maxpool = nullptr;
  ArgMaxProtocol<sci::NetIO, intType> *argmax = nullptr;
  ReLUProtocol<sci::NetIO, intType> *reluArr[MAX_THREADS] = {};
  MaxPoolProtocol<sci::NetIO, intType> *maxpoolArr[MAX_THREADS] = {};
};

struct LinearContext {
#ifdef SCI_OT
  LinearOT *mult = nullptr;
  AuxProtocols *aux = nullptr;
  Truncation *truncation = nullptr;
  XTProtocol *xt = nullptr;
  MathFunctions *math = nullptr;
  MatMulUniform<sci::NetIO, intType, sci::IKNP<sci::NetIO>> *multUniform = nullptr;
  LinearOT *multArr[MAX_THREADS] = {};
  AuxProtocols *auxArr[MAX_THREADS] = {};
  Truncation *truncationArr[MAX_THREADS] = {};
  XTProtocol *xtArr[MAX_THREADS] = {};
  MathFunctions *mathArr[MAX_THREADS] = {};
  MatMulUniform<sci::NetIO, intType, sci::IKNP<sci::NetIO>> *multUniformArr[MAX_THREADS] = {};
#endif

#ifdef SCI_HE
  FCField *he_fc = nullptr;
  ElemWiseProdField *he_prod = nullptr;
#endif

#if USE_CHEETAH
  gemini::CheetahLinear *cheetah_linear = nullptr;
#elif defined(SCI_HE)
  ConvField *he_conv = nullptr;
#endif
};

struct Stats {
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
  uint64_t comm_threads[MAX_THREADS] = {};
  uint64_t num_rounds = 0;

#ifdef LOG_LAYERWISE
  uint64_t ConvTimeInMilliSec = 0;
  uint64_t MatAddTimeInMilliSec = 0;
  uint64_t BatchNormInMilliSec = 0;
  uint64_t TruncationTimeInMilliSec = 0;
  uint64_t ReluTimeInMilliSec = 0;
  uint64_t MaxpoolTimeInMilliSec = 0;
  uint64_t AvgpoolTimeInMilliSec = 0;
  uint64_t MatMulTimeInMilliSec = 0;
  uint64_t MatAddBroadCastTimeInMilliSec = 0;
  uint64_t MulCirTimeInMilliSec = 0;
  uint64_t ScalarMulTimeInMilliSec = 0;
  uint64_t SigmoidTimeInMilliSec = 0;
  uint64_t TanhTimeInMilliSec = 0;
  uint64_t SqrtTimeInMilliSec = 0;
  uint64_t NormaliseL2TimeInMilliSec = 0;
  uint64_t ArgMaxTimeInMilliSec = 0;

  uint64_t ConvCommSent = 0;
  uint64_t MatAddCommSent = 0;
  uint64_t BatchNormCommSent = 0;
  uint64_t TruncationCommSent = 0;
  uint64_t ReluCommSent = 0;
  uint64_t MaxpoolCommSent = 0;
  uint64_t AvgpoolCommSent = 0;
  uint64_t MatMulCommSent = 0;
  uint64_t MatAddBroadCastCommSent = 0;
  uint64_t MulCirCommSent = 0;
  uint64_t ScalarMulCommSent = 0;
  uint64_t SigmoidCommSent = 0;
  uint64_t TanhCommSent = 0;
  uint64_t SqrtCommSent = 0;
  uint64_t NormaliseL2CommSent = 0;
  uint64_t ArgMaxCommSent = 0;

  /**
   * Added by Tanjina - starts
   */

  // for Power readings in microwatts
  uint64_t ConvTotalPowerConsumption = 0;
  uint64_t ReluTotalPowerConsumption = 0;
  uint64_t MaxPoolTotalPowerConsumption = 0;
  uint64_t BatchNormTotalPowerConsumption = 0;
  uint64_t MatMulTotalPowerConsumption = 0;
  uint64_t AvgPoolTotalPowerConsumption = 0;
  uint64_t ArgMaxTotalPowerConsumption = 0;

  // for layer counter
  int Conv_layer_count = 0;
  int Relu_layer_count = 0;
  int MaxPool_layer_count = 0;
  int BatchNorm_layer_count = 0;
  int MatMul_layer_count = 0;
  int AvgPool_layer_count = 0;
  int ArgMax_layer_count = 0;

  // Path to the power usage
  std::string power_usage_path = "/sys/class/hwmon/hwmon4/device/power1_average";

  /* for execution time/duration */
  uint64_t ProtocolStartTime = 0;
  uint64_t ProtocolEndTime = 0;
  uint64_t ProtocolExecutionTime = 0;

  uint64_t ComputationStartTime = 0;
  uint64_t ComputationEndTime = 0;
  uint64_t ComputationExecutionTime = 0;

  uint64_t ConvStartTime = 0;
  uint64_t ConvEndTime = 0;
  uint64_t ConvExecutionTime = 0;

  uint64_t ReluStartTime = 0;
  uint64_t ReluEndTime = 0;
  double ReluExecutionTime = 0.0;

  uint64_t MaxPoolStartTime = 0;
  uint64_t MaxPoolEndTime = 0;
  double MaxPoolExecutionTime = 0.0;

  uint64_t BatchNormStartTime = 0;
  uint64_t BatchNormEndTime = 0;
  double BatchNormExecutionTime = 0.0;

  uint64_t MatMulStartTime = 0;
  uint64_t MatMulEndTime = 0;
  double MatMulExecutionTime = 0.0;

  uint64_t AvgPoolStartTime = 0;
  uint64_t AvgPoolEndTime = 0;
  double AvgPoolExecutionTime = 0.0;

  uint64_t ArgMaxStartTime = 0;
  uint64_t ArgMaxEndTime = 0;
  double ArgMaxExecutionTime = 0.0;

  std::string ProtocolOutputFile =
      "/home/tanjina/OpenCheetah-Tanjina/Output/protocol_output.csv";
  std::vector<std::string> ProtocolHeaders = {
      "index",
      "nn_name",
      "timestamp_power_reading",
      "avg_power_usage_mcW",
      "protocol_start_timestamp",
      "protocol_end_timestamp",
      "protocol_execution_time_ms"};
  WriteToCSV writeProtocolCSV{ProtocolOutputFile, ProtocolHeaders};

  /**
   * Added by Tanjina - ends
   */
#endif  // LOG_LAYERWISE
};

double computeAveragePower(uint64_t totalPower, int layerCount,
                           const std::string &layerName);

class Session {
 public:
  explicit Session(const Config &cfg);
  ~Session();

  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  Session(Session &&) = delete;
  Session &operator=(Session &&) = delete;

  Config cfg;
  IOContext io;
  NonLinearContext nl;
  LinearContext lin;
  Stats stats;
};

}  // namespace sci

extern thread_local sci::Session *g_session;

#endif  // SCI_SESSION_H___
