#ifndef BLUEWHITE_CAN_COMM_HPP_
#define BLUEWHITE_CAN_COMM_HPP_

#include <cstdint>
#include <memory>

#include "bluelink_messages.hpp"
#include "can_messenger.hpp"
#include "comm_interface.hpp"
#include "micro_sequence_executor.hpp"
#include "stm32g4xx_hal.h"

class BluewhiteCanComm {
 public:
  explicit BluewhiteCanComm(FDCAN_HandleTypeDef* bluelink_fdcan, MicroSequenceExecutor* sequence_executor);

  void tick();
  MicroSequenceExecutor& getSequenceExecutor() { return *sequence_executor_; }

 private:
  static uint16_t can_receive_size_;
  static bool can_transmit_flag_;
  static BluewhiteCanComm* interrupt_instance_;

  std::unique_ptr<CommCan> comm_can_;
  std::unique_ptr<CanMessenger> can_messenger_;
  MicroSequenceExecutor* sequence_executor_ = nullptr;
  CanMessenger::RxQueueItem current_rx_item_;

  bool metadata_send_requested_ = false;
  bool bluelink_version_send_requested_ = false;
  uint8_t metadata_destination_id_ = 0;
  uint8_t bluelink_version_destination_id_ = 0;

  InterfaceStatus initializeCanInterface();
  InterfaceStatus configureCanFilter();
  void processRxQueuedMessage();
  void processTxQueue();
  bool tryStartSequenceForMessage(uint8_t payload_type_id, const uint8_t* payload, uint8_t length);
  void directEnqueueRxFromInterrupt(const FDCAN_RxHeaderTypeDef& header, const uint8_t* data, uint8_t length);

  void processProgrammingCommand();
  void processResetCommand();
  void processMetaDataRequest();
  void processBluelinkVersionRequest();
  void handlePendingMetadataRequest();
  void handlePendingBluelinkVersionRequest();
  void requestMetaDataSend(uint8_t destination_id);
  bool sendCanMessage(uint8_t destination_id, bluelink::PayloadTypeIds payload_type, const void* data,
                      size_t data_size);
};

#endif  // BLUEWHITE_CAN_COMM_HPP_
