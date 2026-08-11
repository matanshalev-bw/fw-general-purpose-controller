#ifndef RAW_CAN_INTERFACE_HPP_
#define RAW_CAN_INTERFACE_HPP_

#include <cstdint>
#include <memory>

#include "comm_defines.hpp"
#include "comm_interface.hpp"
#include "interface_status.hpp"

class RawCanInterface {
 public:
  explicit RawCanInterface(CommCanHandle* fdcan_handler);

  InterfaceStatus transmitStandard(uint32_t id, const uint8_t* data, uint8_t dlc);
  // dlc: in = max bytes to copy (1..8), out = actual bytes copied from matching frame.
  InterfaceStatus receiveStandard(uint32_t id, uint8_t* data, uint8_t& dlc, uint32_t timeout_ms);

 private:
  static uint16_t can_receive_size_;
  static bool can_transmit_flag_;

  std::unique_ptr<CommCan> comm_can_;
  CommCanHandle* handler_ = nullptr;

  InterfaceStatus initializeCanInterface();
};

#endif  // RAW_CAN_INTERFACE_HPP_
