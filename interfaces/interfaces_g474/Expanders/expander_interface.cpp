#include "expander_interface.hpp"

#include "ads7953_driver.hpp"
#include "dac7578_driver.hpp"
#include "mcp23x17_driver.hpp"
#include "pca9685_driver.hpp"

namespace {

Mcp23x17Driver g_gpio_i2c(Mcp23x17Driver::Bus::I2C);
Mcp23x17Driver g_gpio_spi(Mcp23x17Driver::Bus::SPI);
Ads7953Driver g_adc;
Dac7578Driver g_dac;
Pca9685Driver g_pwm;

}  // namespace

InterfaceStatus ExpanderInterface::initAll() {
  (void)g_gpio_i2c.init();
  (void)g_gpio_spi.init();
  (void)g_adc.init();
  (void)g_dac.init();
  (void)g_pwm.init();
  return InterfaceStatus::INTERFACE_OK;
}

GpioExpanderInterface& ExpanderInterface::gpio(GpioKind kind) {
  return (kind == GpioKind::SPI) ? static_cast<GpioExpanderInterface&>(g_gpio_spi)
                                 : static_cast<GpioExpanderInterface&>(g_gpio_i2c);
}

AdcExpanderInterface& ExpanderInterface::adc() { return g_adc; }

DacExpanderInterface& ExpanderInterface::dac() { return g_dac; }

PwmExpanderInterface& ExpanderInterface::pwm() { return g_pwm; }
