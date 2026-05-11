#include "session.h"

#include <utility>

namespace sci {

Session::Session(const Config &cfg_) : cfg(cfg_) {
  for (int i = 0; i < cfg.num_threads; i++) {
    io.ioArr[i] = new sci::NetIO(
        cfg.party == sci::ALICE ? nullptr : cfg.address.c_str(),
        cfg.port + i, /*quit*/ true);
    io.otInstanceArr[i] = new sci::IKNP<sci::NetIO>(io.ioArr[i]);
    io.prgInstanceArr[i] = new sci::PRG128();
    io.kkotInstanceArr[i] = new sci::KKOT<sci::NetIO>(io.ioArr[i]);
#ifdef SCI_OT
    lin.multUniformArr[i] =
        new MatMulUniform<sci::NetIO, intType, sci::IKNP<sci::NetIO>>(
            cfg.party, cfg.bitlength, io.ioArr[i], io.otInstanceArr[i],
            nullptr);
#endif
    if (i & 1) {
      io.otpackArr[i] =
          new sci::OTPack<sci::NetIO>(io.ioArr[i], 3 - cfg.party);
    } else {
      io.otpackArr[i] = new sci::OTPack<sci::NetIO>(io.ioArr[i], cfg.party);
    }
  }

  io.primary = io.ioArr[0];
  io.otpack = io.otpackArr[0];
  io.iknpOT = new sci::IKNP<sci::NetIO>(io.primary);
  io.iknpOTRoleReversed = new sci::IKNP<sci::NetIO>(io.primary);
  io.kkot = new sci::KKOT<sci::NetIO>(io.primary);
  io.prg128 = new sci::PRG128();

#ifdef SCI_OT
  lin.multArr[0] = new LinearOT(cfg.party, io.primary, io.otpack);
  lin.mult = lin.multArr[0];
  lin.truncation = new Truncation(cfg.party, io.primary, io.otpack);
  lin.multUniform =
      new MatMulUniform<sci::NetIO, intType, sci::IKNP<sci::NetIO>>(
          cfg.party, cfg.bitlength, io.primary, io.iknpOT,
          io.iknpOTRoleReversed);
  nl.relu = new ReLURingProtocol<sci::NetIO, intType>(
      cfg.party, RING, io.primary, cfg.bitlength, MILL_PARAM, io.otpack);
  nl.maxpool = new MaxPoolProtocol<sci::NetIO, intType>(
      cfg.party, RING, io.primary, cfg.bitlength, MILL_PARAM, 0, io.otpack,
      nl.relu);
  nl.argmax = new ArgMaxProtocol<sci::NetIO, intType>(
      cfg.party, RING, io.primary, cfg.bitlength, MILL_PARAM, 0, io.otpack,
      nl.relu);
  lin.math = new MathFunctions(cfg.party, io.primary, io.otpack);
#endif

#if USE_CHEETAH
  lin.cheetah_linear = new gemini::CheetahLinear(
      cfg.party, io.primary, cfg.prime_mod, cfg.num_threads);
#elif defined(SCI_HE)
  lin.he_conv = new ConvField(cfg.party, io.primary);
#endif

#ifdef SCI_HE
  nl.relu = new ReLUFieldProtocol<sci::NetIO, intType>(
      cfg.party, FIELD, io.primary, cfg.bitlength, MILL_PARAM, cfg.prime_mod,
      io.otpack);
  nl.maxpool = new MaxPoolProtocol<sci::NetIO, intType>(
      cfg.party, FIELD, io.primary, cfg.bitlength, MILL_PARAM, cfg.prime_mod,
      io.otpack, nl.relu);
  nl.argmax = new ArgMaxProtocol<sci::NetIO, intType>(
      cfg.party, FIELD, io.primary, cfg.bitlength, MILL_PARAM, cfg.prime_mod,
      io.otpack, nl.relu);
  lin.he_fc = new FCField(cfg.party, io.primary);
  lin.he_prod = new ElemWiseProdField(cfg.party, io.primary);
  assertFieldRun();
#endif

#if defined(MULTITHREADED_NONLIN) && defined(SCI_OT)
  for (int i = 0; i < cfg.num_threads; i++) {
    if (i & 1) {
      nl.reluArr[i] = new ReLURingProtocol<sci::NetIO, intType>(
          3 - cfg.party, RING, io.ioArr[i], cfg.bitlength, MILL_PARAM,
          io.otpackArr[i]);
      nl.maxpoolArr[i] = new MaxPoolProtocol<sci::NetIO, intType>(
          3 - cfg.party, RING, io.ioArr[i], cfg.bitlength, MILL_PARAM, 0,
          io.otpackArr[i], nl.reluArr[i]);
      if (lin.multArr[i] == nullptr) {
        lin.multArr[i] = new LinearOT(3 - cfg.party, io.ioArr[i], io.otpackArr[i]);
      }
      lin.truncationArr[i] =
          new Truncation(3 - cfg.party, io.ioArr[i], io.otpackArr[i]);
    } else {
      nl.reluArr[i] = new ReLURingProtocol<sci::NetIO, intType>(
          cfg.party, RING, io.ioArr[i], cfg.bitlength, MILL_PARAM,
          io.otpackArr[i]);
      nl.maxpoolArr[i] = new MaxPoolProtocol<sci::NetIO, intType>(
          cfg.party, RING, io.ioArr[i], cfg.bitlength, MILL_PARAM, 0,
          io.otpackArr[i], nl.reluArr[i]);
      if (lin.multArr[i] == nullptr) {
        lin.multArr[i] = new LinearOT(cfg.party, io.ioArr[i], io.otpackArr[i]);
      }
      lin.truncationArr[i] =
          new Truncation(cfg.party, io.ioArr[i], io.otpackArr[i]);
    }
  }
#endif

#ifdef SCI_HE
  for (int i = 0; i < cfg.num_threads; i++) {
    if (i & 1) {
      nl.reluArr[i] = new ReLUFieldProtocol<sci::NetIO, intType>(
          3 - cfg.party, FIELD, io.ioArr[i], cfg.bitlength, MILL_PARAM,
          cfg.prime_mod, io.otpackArr[i]);
      nl.maxpoolArr[i] = new MaxPoolProtocol<sci::NetIO, intType>(
          3 - cfg.party, FIELD, io.ioArr[i], cfg.bitlength, MILL_PARAM,
          cfg.prime_mod, io.otpackArr[i], nl.reluArr[i]);
    } else {
      nl.reluArr[i] = new ReLUFieldProtocol<sci::NetIO, intType>(
          cfg.party, FIELD, io.ioArr[i], cfg.bitlength, MILL_PARAM,
          cfg.prime_mod, io.otpackArr[i]);
      nl.maxpoolArr[i] = new MaxPoolProtocol<sci::NetIO, intType>(
          cfg.party, FIELD, io.ioArr[i], cfg.bitlength, MILL_PARAM,
          cfg.prime_mod, io.otpackArr[i], nl.reluArr[i]);
    }
  }
#endif

#ifdef SCI_OT
  for (int i = 0; i < cfg.num_threads; i++) {
    if (i & 1) {
      lin.auxArr[i] =
          new AuxProtocols(3 - cfg.party, io.ioArr[i], io.otpackArr[i]);
      lin.truncationArr[i] = new Truncation(3 - cfg.party, io.ioArr[i],
                                            io.otpackArr[i], lin.auxArr[i]);
      lin.xtArr[i] = new XTProtocol(3 - cfg.party, io.ioArr[i],
                                    io.otpackArr[i], lin.auxArr[i]);
      lin.mathArr[i] =
          new MathFunctions(3 - cfg.party, io.ioArr[i], io.otpackArr[i]);
      if (lin.multArr[i] == nullptr) {
        lin.multArr[i] = new LinearOT(3 - cfg.party, io.ioArr[i], io.otpackArr[i]);
      }
    } else {
      lin.auxArr[i] =
          new AuxProtocols(cfg.party, io.ioArr[i], io.otpackArr[i]);
      lin.truncationArr[i] = new Truncation(cfg.party, io.ioArr[i],
                                            io.otpackArr[i], lin.auxArr[i]);
      lin.xtArr[i] = new XTProtocol(cfg.party, io.ioArr[i], io.otpackArr[i],
                                    lin.auxArr[i]);
      lin.mathArr[i] =
          new MathFunctions(cfg.party, io.ioArr[i], io.otpackArr[i]);
      if (lin.multArr[i] == nullptr) {
        lin.multArr[i] = new LinearOT(cfg.party, io.ioArr[i], io.otpackArr[i]);
      }
    }
  }
  lin.aux = lin.auxArr[0];
  lin.truncation = lin.truncationArr[0];
  lin.xt = lin.xtArr[0];
  lin.mult = lin.multArr[0];
  lin.math = lin.mathArr[0];
#endif

  if (cfg.party == sci::ALICE) {
    io.iknpOT->setup_send();
    io.iknpOTRoleReversed->setup_recv();
  } else if (cfg.party == sci::BOB) {
    io.iknpOT->setup_recv();
    io.iknpOTRoleReversed->setup_send();
  }
}

Session::~Session() {
#ifdef SCI_OT
  for (int i = 0; i < cfg.num_threads; i++) {
    delete lin.mathArr[i];
    delete lin.xtArr[i];
    delete lin.truncationArr[i];
    delete lin.auxArr[i];
    delete lin.multArr[i];
    delete lin.multUniformArr[i];
  }
  delete lin.multUniform;
#endif

#ifdef SCI_HE
  delete lin.he_prod;
  delete lin.he_fc;
#endif

#if USE_CHEETAH
  delete lin.cheetah_linear;
#elif defined(SCI_HE)
  delete lin.he_conv;
#endif

  delete nl.argmax;
  delete nl.maxpool;
  delete nl.relu;
  for (int i = 0; i < cfg.num_threads; i++) {
    delete nl.maxpoolArr[i];
    delete nl.reluArr[i];
  }

  delete io.prg128;
  delete io.kkot;
  delete io.iknpOTRoleReversed;
  delete io.iknpOT;
  for (int i = 0; i < cfg.num_threads; i++) {
#if !USE_CHEETAH
    delete io.otpackArr[i];
#endif
    delete io.kkotInstanceArr[i];
    delete io.prgInstanceArr[i];
    delete io.otInstanceArr[i];
    delete io.ioArr[i];
  }
#if USE_CHEETAH
  delete io.otpackArr[0];
#endif
}

} // namespace sci

sci::Session *g_session = nullptr;
