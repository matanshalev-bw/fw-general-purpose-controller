#ifndef RAW_CAN_INTERFACE_HPP_
#define RAW_CAN_INTERFACE_HPP_

#include <cstdint>

#include "interface_status.hpp"

class RawCanInterface {
 public:
  explicit RawCanInterface(void* fdcan_handler);

  InterfaceStatus transmitStandard(uint32_t id, const uint8_t* data, uint8_t dlc);

 private:
  void* handler_;
};

#endif  // RAW_CAN_INTERFACE_HPP_
