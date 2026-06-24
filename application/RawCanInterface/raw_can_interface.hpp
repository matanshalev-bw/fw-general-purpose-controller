#ifndef RAW_CAN_INTERFACE_HPP_
#define RAW_CAN_INTERFACE_HPP_

#include <cstdint>

#include "comm_defines.hpp"
#include "interface_status.hpp"

class RawCanInterface {
 public:
  explicit RawCanInterface(CommCanHandle* fdcan_handler);

  InterfaceStatus transmitStandard(uint32_t id, const uint8_t* data, uint8_t dlc);

 private:
  CommCanHandle* handler_;
};

#endif  // RAW_CAN_INTERFACE_HPP_
