/*
 * spi_comm_defines.hpp
 *
 * Created on: Jul 23, 2025
 * Author: ariel
 */

#ifndef SRC_SPI_COMM_DEFINES_HPP_
#define SRC_SPI_COMM_DEFINES_HPP_

#include <stdint.h>

namespace SpiComm {

enum class AuxStatusFlags : uint8_t {
    BYPASS_ENABLED_FLAG = 0x01,        // Bypass relay is currently enabled
    FIRST_AUX_COMM_FLAG = 0x02,        // First valid SPI communication established
    EMERGENCY_SEQUENCE_ACTIVE = 0x04,  // Emergency sequence currently running on aux
    AUX_SW_VERSION_OK_FLAG = 0x08,     // Aux software version is compatible/known
    RESERVED_FLAG_4 = 0x10,            // Reserved for future use
    RESERVED_FLAG_5 = 0x20,            // Reserved for future use
    RESERVED_FLAG_6 = 0x40,            // Reserved for future use
    RESERVED_FLAG_7 = 0x80             // Reserved for future use
};

enum class MasterStatusFlags : uint8_t {
    MAIN_SYSTEM_HEALTHY_FLAG = 0x01,   // Main system health status
    EMERGENCY_OVERRIDE_FLAG = 0x02,    // Emergency override active
    MANUAL_MODE_ACTIVE_FLAG = 0x04,    // Manual operation mode active
    RESERVED_FLAG_4 = 0x08,            // Reserved for future use
    RESERVED_FLAG_5 = 0x10,            // Reserved for future use
    RESERVED_FLAG_6 = 0x20,            // Reserved for future use
    RESERVED_FLAG_7 = 0x40,            // Reserved for future use
    RESERVED_FLAG_8 = 0x80             // Reserved for future use
};

struct MasterTxMessage {
    uint8_t aux_command;      // AuxCommand enum
    uint8_t main_sw_version;  // Software version
    uint8_t status_flags;     // Master status flags (reserved for future)
    uint8_t crc8;            // CRC over first 3 bytes
};

struct AuxRxMessage {
    uint8_t aux_state;        // AuxState enum
    uint8_t aux_sw_version;   // Software version
    uint8_t status_flags;     // Aux status flags
    uint8_t crc8;            // CRC over first 3 bytes
};

} // namespace SpiComm

#endif /* SRC_SPI_COMM_DEFINES_HPP_ */
