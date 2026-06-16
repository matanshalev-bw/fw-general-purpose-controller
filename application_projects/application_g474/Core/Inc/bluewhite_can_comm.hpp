#ifndef BLUEWHITE_CAN_COMM_HPP_
#define BLUEWHITE_CAN_COMM_HPP_

#include <cstdint>
#include <memory>

#include "bluewhite_message_handler.hpp"
#include "can_messenger.hpp"
#include "comm_interface.hpp"
#include "gpc_controller.hpp"
#include "micro_sequence_executor.hpp"
#include "stm32g4xx_hal.h"

class BluewhiteCanComm {
 public:
  explicit BluewhiteCanComm(FDCAN_HandleTypeDef* bluelink_fdcan, MicroSequenceExecutor* sequence_executor,
                            GpcController* gpc_controller = nullptr);

  void tick();
  MicroSequenceExecutor& getSequenceExecutor() { return *sequence_executor_; }
  CommCan* bootloaderComm() { return comm_can_.get(); }
  bool sendTelemetry(uint8_t destination_id, bluelink::PayloadTypeIds payload_type, const void* data, size_t data_size);

 private:
  static uint16_t can_receive_size_;
  static bool can_transmit_flag_;
  static BluewhiteCanComm* interrupt_instance_;

  MicroSequenceExecutor* sequence_executor_ = nullptr;
  std::unique_ptr<CommCan> comm_can_;
  BluewhiteMessageHandler message_handler_;
  std::unique_ptr<CanMessenger> can_messenger_;
  CanMessenger::RxQueueItem current_rx_item_;

  InterfaceStatus initializeCanInterface();
  InterfaceStatus configureCanFilter();
  void processRxQueuedMessage();
  void processTxQueue();
  void directEnqueueRxFromInterrupt(const FDCAN_RxHeaderTypeDef& header, const uint8_t* data, uint8_t length);

  bool sendCanMessage(uint8_t destination_id, bluelink::PayloadTypeIds payload_type, const void* data, size_t data_size);
};

#endif  // BLUEWHITE_CAN_COMM_HPP_
