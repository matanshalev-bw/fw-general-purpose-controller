#ifndef RAW_CAN_INTERFACE_HPP_
#define RAW_CAN_INTERFACE_HPP_

#include <cstdint>
#include "interface_status.hpp"
#include "stm32g4xx_hal.h"

class RawCanInterface {
 public:
  explicit RawCanInterface(FDCAN_HandleTypeDef* handler);

  InterfaceStatus transmitStandard(uint32_t id, const uint8_t* data, uint8_t dlc);

 private:
  FDCAN_HandleTypeDef* handler_;
};

#endif  // RAW_CAN_INTERFACE_HPP_
