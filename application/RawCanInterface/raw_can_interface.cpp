#include "raw_can_interface.hpp"

#include "comm_interface.hpp"

RawCanInterface::RawCanInterface(void* fdcan_handler) : handler_(fdcan_handler) {
  CommCan::startPeripheral(static_cast<FDCAN_HandleTypeDef*>(handler_));
}

InterfaceStatus RawCanInterface::transmitStandard(uint32_t id, const uint8_t* data, uint8_t dlc) {
  return CommCan::transmitStandard(static_cast<FDCAN_HandleTypeDef*>(handler_), id, data, dlc);
}
