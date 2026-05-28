/*
 * led_status_handler.hpp
 */

#ifndef SRC_LED_STATUS_HANDLER_HPP_
#define SRC_LED_STATUS_HANDLER_HPP_

#include <stdint.h>
#include "main.h"
#include "gpio_interface.hpp"
#include "bootloader_defines.hpp"

class LedStatusHandler {
public:
    LedStatusHandler();
    void updateForBootloaderState(BootloaderState state);

private:
    enum class LedPattern : uint8_t {
        OFF = 0,
        ON = 1,
        SLOW_BLINK = 2,
        FAST_BLINK = 3
    };

    static constexpr uint32_t SLOW_BLINK_TICKS = 20;
    static constexpr uint32_t FAST_BLINK_TICKS = 10;

    GpioPin comm_led_gpio_;
    bool comm_led_state_ = false;
    uint32_t comm_tick_counter_ = 0;

    inline LedPattern getCommunicationPatternForState(BootloaderState state) const;
    bool calculatePatternState(LedPattern pattern, uint32_t& tick_counter) const;
};

#endif /* SRC_LED_STATUS_HANDLER_HPP_ */
