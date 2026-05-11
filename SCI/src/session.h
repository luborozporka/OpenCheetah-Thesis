#ifndef SCI_SESSION_H___
#define SCI_SESSION_H___

#include "NonLinear/argmax.h"
#include "NonLinear/maxpool.h"
#include "NonLinear/relu-interface.h"
#include "OT/kkot.h"
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
};

}  // namespace sci

extern sci::Session *g_session;

#endif  // SCI_SESSION_H___
