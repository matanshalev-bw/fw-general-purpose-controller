#ifndef FW_SEQUENCESCONFIGUARTIONS_HPP_
#define FW_SEQUENCESCONFIGUARTIONS_HPP_

#include "gpio_defines.hpp"

/**
** Do not edit if you don't have to!
** Default parameters. For others, change the specific config handler
**/

#define SEQUENCE_MAX_STEPS 10
#define PINS_PER_STEP 4

struct GpioConfig {
    GpioPortType port;
    GpioPinNumber pin;
    GpioPinState state;
} __attribute__((packed));

struct Step {
    GpioConfig gpios[PINS_PER_STEP];
} __attribute__((packed));

struct Sequence {
    Step steps[SEQUENCE_MAX_STEPS];
} __attribute__((packed));

struct SequencesConfig {
    uint16_t delay_between_steps = 100;

    // Forward sequence steps (NH Open Cabin default)
    Sequence forward_sequence = {
        .steps = {
            { // STATE_0
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_15, GpioPinState::PIN_SET}, // CH_H = SET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // STATE_1
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_15, GpioPinState::PIN_RESET}, // CH_H = RESET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // STATE_2
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_14, GpioPinState::PIN_SET}, // CH_F = SET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // STATE_3
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_1, GpioPinState::PIN_SET}, // CH_A = SET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // STATE_4
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_1, GpioPinState::PIN_RESET}, // CH_A = RESET
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_14, GpioPinState::PIN_RESET}, // CH_F = RESET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // End of sequence
                .gpios = {
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            }
        }
    };

    // Reverse sequence steps (NH Open Cabin default)
    Sequence reverse_sequence = {
        .steps = {
            { // STATE_0
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_15, GpioPinState::PIN_SET}, // CH_H = SET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // STATE_1
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_15, GpioPinState::PIN_RESET}, // CH_H = RESET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // STATE_2
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_14, GpioPinState::PIN_SET}, // CH_F = SET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // STATE_3
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_2, GpioPinState::PIN_SET}, // CH_B = SET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // STATE_4
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_2, GpioPinState::PIN_RESET}, // CH_B = RESET
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_14, GpioPinState::PIN_RESET}, // CH_F = RESET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // End of sequence
                .gpios = {
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            }
        }
    };

    // Neutral sequence steps (NH Open Cabin default)
    Sequence neutral_sequence = {
        .steps = {
            { // STATE_0
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_15, GpioPinState::PIN_SET}, // CH_H = SET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // STATE_1
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_15, GpioPinState::PIN_RESET}, // CH_H = RESET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // End of sequence
                .gpios = {
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            }
        }
    };

    // Pass through sequence steps (NH Open Cabin default)
    Sequence pass_trough_sequence = {
        .steps = {
            { // STATE_0
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_15, GpioPinState::PIN_SET}, // CH_H = SET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // STATE_1
                .gpios = {
                    {GpioPortType::PORT_B, GpioPinNumber::PIN_15, GpioPinState::PIN_RESET}, // CH_H = RESET
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            },
            { // End of sequence
                .gpios = {
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET},
                    {GpioPortType::UNKNOWN_PORT, GpioPinNumber::PIN_0, GpioPinState::PIN_RESET}
                }
            }
        }
    };
} __attribute__((packed));

#endif // FW_SEQUENCESCONFIGUARTIONS_HPP_
