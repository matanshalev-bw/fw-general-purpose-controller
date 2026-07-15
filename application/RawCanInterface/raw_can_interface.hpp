#ifndef RAW_CAN_INTERFACE_HPP_
#define RAW_CAN_INTERFACE_HPP_

#include <cstdint>

#include "comm_defines.hpp"
#include "interface_status.hpp"

class RawCanInterface {
 public:
  explicit RawCanInterface(CommCanHandle* fdcan_handler);

  InterfaceStatus transmitStandard(uint32_t id, const uint8_t* data, uint8_t dlc);
  // dlc: in = max bytes to copy (1..8), out = actual bytes copied from matching frame.
  InterfaceStatus receiveStandard(uint32_t id, uint8_t* data, uint8_t& dlc, uint32_t timeout_ms);

 private:
  CommCanHandle* handler_;
};

#endif  // RAW_CAN_INTERFACE_HPP_
