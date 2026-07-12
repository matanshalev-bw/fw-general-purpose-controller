#ifndef SAFETY_FEATURES_HPP_
#define SAFETY_FEATURES_HPP_

#include <cstdint>

#include "gpio_interface.hpp"
#include "hardware_map.hpp"
#include "system_interface.hpp"

class SafetyFeatures {
 public:
  void initialize();
  void tick();

 private:
  static constexpr uint32_t WD_UPPER_MS = 1000;
  static constexpr uint32_t WD_LOWER_MS = 400;

  void powerupWdEn();
  void tickWdKa();

  GpioPin wd_en_;
  GpioPin wd_ka_;
  bool ka_high_ = false;
  uint32_t ka_phase_start_ms_ = 0;
};

#endif  // SAFETY_FEATURES_HPP_
