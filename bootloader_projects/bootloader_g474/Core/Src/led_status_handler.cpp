/*
 * led_status_handler.cpp
 */

#include "led_status_handler.hpp"

LedStatusHandler::LedStatusHandler()
    : comm_led_gpio_(GpioInterface::createDigitalGpio(GREEN_LED_1_GPIO_Port, GREEN_LED_1_Pin)) {
    GpioInterface::digitalWrite(comm_led_gpio_, GpioPinState::PIN_RESET);
}

void LedStatusHandler::updateForBootloaderState(BootloaderState state) {
    LedPattern comm_pattern = getCommunicationPatternForState(state);

    bool new_comm_state = calculatePatternState(comm_pattern, comm_tick_counter_);

    if (new_comm_state != comm_led_state_) {
        comm_led_state_ = new_comm_state;
        GpioInterface::digitalWrite(comm_led_gpio_, comm_led_state_ ? GpioPinState::PIN_SET : GpioPinState::PIN_RESET);
    }
}

inline LedStatusHandler::LedPattern LedStatusHandler::getCommunicationPatternForState(BootloaderState state) const {
    switch (state) {
        case BootloaderState::INIT:
        case BootloaderState::WAITING_FOR_COMMAND:
        case BootloaderState::ERROR_STATE:
            return LedPattern::SLOW_BLINK;
        case BootloaderState::PROGRAMMING_READY:
        case BootloaderState::WAITING_FOR_PROGRAMMING_READY:
        case BootloaderState::PROGRAMMING_IN_PROGRESS:
            return LedPattern::FAST_BLINK;
        case BootloaderState::PROGRAMMING_COMPLETE:
            return LedPattern::ON;
        case BootloaderState::JUMP_TO_APP:
            return LedPattern::OFF;
        default:
            return LedPattern::OFF;
    }
}

bool LedStatusHandler::calculatePatternState(LedPattern pattern, uint32_t& tick_counter) const {
    ++tick_counter;

    switch (pattern) {
        case LedPattern::OFF: return false;
        case LedPattern::ON: return true;
        case LedPattern::SLOW_BLINK:
            if (tick_counter >= SLOW_BLINK_TICKS) tick_counter = 0;
            return tick_counter < (SLOW_BLINK_TICKS / 2);
        case LedPattern::FAST_BLINK:
            if (tick_counter >= FAST_BLINK_TICKS) tick_counter = 0;
            return tick_counter < (FAST_BLINK_TICKS / 2);
        default: return false;
    }
}
