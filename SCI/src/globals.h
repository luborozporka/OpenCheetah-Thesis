/*
Authors: Nishant Kumar, Deevashwer Rathee
Modified by Wen-jie Lu
Copyright:
Copyright (c) 2021 Microsoft Research
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef GLOBALS_H___
#define GLOBALS_H___

#include "csv_writer.hpp"
#include "session.h"

#include <chrono>
#include <cstdint>
#include <thread>

#if USE_CHEETAH
extern bool kIsSharedInput;
#endif

extern std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
extern uint64_t comm_threads[MAX_THREADS];
extern uint64_t num_rounds;

#ifdef LOG_LAYERWISE
extern uint64_t ConvTimeInMilliSec;
extern uint64_t MatAddTimeInMilliSec;
extern uint64_t BatchNormInMilliSec;
extern uint64_t TruncationTimeInMilliSec;
extern uint64_t ReluTimeInMilliSec;
extern uint64_t MaxpoolTimeInMilliSec;
extern uint64_t AvgpoolTimeInMilliSec;
extern uint64_t MatMulTimeInMilliSec;
extern uint64_t MatAddBroadCastTimeInMilliSec;
extern uint64_t MulCirTimeInMilliSec;
extern uint64_t ScalarMulTimeInMilliSec;
extern uint64_t SigmoidTimeInMilliSec;
extern uint64_t TanhTimeInMilliSec;
extern uint64_t SqrtTimeInMilliSec;
extern uint64_t NormaliseL2TimeInMilliSec;
extern uint64_t ArgMaxTimeInMilliSec;

extern uint64_t ConvCommSent;
extern uint64_t MatAddCommSent;
extern uint64_t BatchNormCommSent;
extern uint64_t TruncationCommSent;
extern uint64_t ReluCommSent;
extern uint64_t MaxpoolCommSent;
extern uint64_t AvgpoolCommSent;
extern uint64_t MatMulCommSent;
extern uint64_t MatAddBroadCastCommSent;
extern uint64_t MulCirCommSent;
extern uint64_t ScalarMulCommSent;
extern uint64_t SigmoidCommSent;
extern uint64_t TanhCommSent;
extern uint64_t SqrtCommSent;
extern uint64_t NormaliseL2CommSent;
extern uint64_t ArgMaxCommSent;

/**
 * Added by Tanjina - starts
 */

// for Power readings in microwatts
extern uint64_t ConvTotalPowerConsumption;
extern uint64_t ReluTotalPowerConsumption;
extern uint64_t MaxPoolTotalPowerConsumption;
extern uint64_t BatchNormTotalPowerConsumption;
extern uint64_t MatMulTotalPowerConsumption;
extern uint64_t AvgPoolTotalPowerConsumption;
extern uint64_t ArgMaxTotalPowerConsumption;

// for layer counter
extern int Conv_layer_count;
extern int Relu_layer_count;
extern int MaxPool_layer_count;
extern int BatchNorm_layer_count;
extern int MatMul_layer_count;
extern int AvgPool_layer_count;
extern int ArgMax_layer_count;

// Path to the power usage
extern std::string power_usage_path;

// Added by Tanjina
double computeAveragePower(uint64_t totalPower, int layerCount, const std::string& layerName);

/* for execution time/duration */
extern uint64_t ProtocolStartTime;
extern uint64_t ProtocolEndTime;
extern uint64_t ProtocolExecutionTime;

extern uint64_t ConvStartTime;
extern uint64_t ConvEndTime;
extern uint64_t ConvExecutionTime;

extern uint64_t ReluStartTime;
extern uint64_t ReluEndTime;
extern double ReluExecutionTime;

extern uint64_t MaxPoolStartTime;
extern uint64_t MaxPoolEndTime;
extern double MaxPoolExecutionTime;

extern uint64_t BatchNormStartTime;
extern uint64_t BatchNormEndTime;
extern double BatchNormExecutionTime;

extern uint64_t MatMulStartTime;
extern uint64_t MatMulEndTime;
extern double MatMulExecutionTime;

extern uint64_t AvgPoolStartTime;
extern uint64_t AvgPoolEndTime;
extern double AvgPoolExecutionTime;

extern uint64_t ArgMaxStartTime;
extern uint64_t ArgMaxEndTime;
extern double ArgMaxExecutionTime;

extern std::string ProtocolOutputFile;
extern std::vector<std::string> ProtocolHeaders; 
extern WriteToCSV writeProtocolCSV;

//extern std::string layerType;
// extern std::string ConvOutputFile;
// extern std::vector<std::string> ConvHeaders; 
// extern WriteToCSV writeConvCSV;

// extern std::string ReluOutputFile;
// extern std::vector<std::string> ReluHeaders; 
// extern WriteToCSV writeReluCSV;

// extern std::string MaxPoolOutputFile;
// extern std::vector<std::string> MaxPoolHeaders; 
// extern WriteToCSV writeMaxPoolCSV;

// extern std::string BatchNorm1OutputFile;
// extern std::vector<std::string> BatchNorm1Headers; 
// extern WriteToCSV writeBatchNorm1CSV;

/**
 * Added by Tanjina - ends
 */

#endif

#endif // GLOBALS_H__
