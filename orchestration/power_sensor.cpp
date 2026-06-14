#include "orchestration/power_sensor.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace orchestration {
namespace {

class FilePowerSensor final : public PowerSensor {
 public:
  explicit FilePowerSensor(std::string path) : path_(std::move(path)) {}

  PowerReading Read() override {
    std::ifstream input(path_);
    if (!input) {
      throw std::runtime_error("cannot open power sensor: " + path_);
    }

    int64_t microwatts = 0;
    if (!(input >> microwatts)) {
      throw std::runtime_error("cannot read power sensor: " + path_);
    }

    PowerReading reading;
    reading.available = true;
    reading.power_w = static_cast<double>(microwatts) / 1000000.0;
    return reading;
  }

 private:
  std::string path_;
};

class NoPowerSensor final : public PowerSensor {
 public:
  PowerReading Read() override { return {}; }
};

}

std::unique_ptr<PowerSensor> CreatePowerSensor(const std::string &power_path,
                                               bool allow_no_power_sensor) {
  if (!power_path.empty()) {
    return std::make_unique<FilePowerSensor>(power_path);
  }
  if (allow_no_power_sensor) {
    return std::make_unique<NoPowerSensor>();
  }

  throw std::runtime_error("power_path is required unless allow_no_power_sensor=1");
}

}  // namespace orchestration
