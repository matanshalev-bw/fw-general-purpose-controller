#include "raw_can_interface.hpp"

#include "comm_interface.hpp"

RawCanInterface::RawCanInterface(CommCanHandle* fdcan_handler) : handler_(fdcan_handler) {
  CommCan::startPeripheral(handler_);
}

InterfaceStatus RawCanInterface::transmitStandard(uint32_t id, const uint8_t* data, uint8_t dlc) {
  return CommCan::transmitStandard(handler_, id, data, dlc);
}

InterfaceStatus RawCanInterface::receiveStandard(uint32_t id, uint8_t* data, uint8_t& dlc,
                                                 uint32_t timeout_ms) {
  return CommCan::receiveStandard(handler_, id, data, dlc, timeout_ms);
}
