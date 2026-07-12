#include "safety_features.hpp"

void SafetyFeatures::initialize() {
  wd_en_ = GpioInterface::createDigitalGpio(HardwareMap::WD_EN_PORT, HardwareMap::WD_EN_PIN);
  wd_ka_ = GpioInterface::createDigitalGpio(HardwareMap::WD_KA_PORT, HardwareMap::WD_KA_PIN);

  powerupWdEn();

  GpioInterface::digitalWrite(wd_ka_, GpioPinState::PIN_SET);
  ka_high_ = true;
  ka_phase_start_ms_ = SystemInterface::getTick();
}

void SafetyFeatures::tick() {
  tickWdKa();
}

void SafetyFeatures::powerupWdEn() {
  GpioInterface::digitalWrite(wd_en_, GpioPinState::PIN_SET);
  GpioInterface::digitalWrite(wd_en_, GpioPinState::PIN_RESET);
}

void SafetyFeatures::tickWdKa() {
  const uint32_t phase_ms = ka_high_ ? WD_UPPER_MS : WD_LOWER_MS;
  if ((SystemInterface::getTick() - ka_phase_start_ms_) < phase_ms) {
    return;
  }

  ka_high_ = !ka_high_;
  GpioInterface::digitalWrite(wd_ka_, ka_high_ ? GpioPinState::PIN_SET : GpioPinState::PIN_RESET);
  ka_phase_start_ms_ = SystemInterface::getTick();
}
