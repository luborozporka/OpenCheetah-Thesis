#ifndef ORCHESTRATION_POWER_SENSOR_H_
#define ORCHESTRATION_POWER_SENSOR_H_

#include <memory>
#include <string>

namespace orchestration {

struct PowerReading {
  bool available = false;
  double power_w = 0.0;
};

class PowerSensor {
 public:
  virtual ~PowerSensor() = default;
  virtual PowerReading Read() = 0;
};

std::unique_ptr<PowerSensor> CreatePowerSensor(const std::string &power_path, bool allow_no_power_sensor);

}

#endif
