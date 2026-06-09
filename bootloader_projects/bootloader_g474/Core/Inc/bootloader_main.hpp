/*
 * bootloader_main.hpp
 *
 * Created on: Apr 2, 2025
 * Author: matan
 */

#ifndef SRC_BOOTLOADER_MAIN_HPP_
#define SRC_BOOTLOADER_MAIN_HPP_

#include <memory>

#include "main.h"
#include "comm_interface.hpp"
#include "non_volatile_memory_interface.hpp"
#include "bluelink_messages.hpp"
#include "distributed_can_id.hpp"
#include "memory_map.hpp"
#include "config_memory.hpp"
#include "meta_data.hpp"
#include "system_interface.hpp"
#include "can_messenger.hpp"
#include "gpio_interface.hpp"
#include "scheduler_interface.hpp"
#include "bootloader_defines.hpp"
#include "bootloader_usb_comm.hpp"
#include "led_status_handler.hpp"

#define IS_APP_EXIST(app_addr) (((*(uint32_t*) app_addr) & 0x2FFE0000) == 0x20000000)
typedef void (*pFunction)(void);

class BootloaderMain {
public:
 private:


  static constexpr uint16_t PROGRAMMING_TIMEOUT_MS_ = 3000;
  static constexpr uint16_t BOOT_TIMEOUT_MS_ = 3000;
  static constexpr uint16_t FORCE_APP_TIMEOUT_MS_ = 5000;
  static constexpr uint16_t FLASH_ERASE_DELAY_MS_ = 1500;
  static constexpr uint16_t REQUEST_PERIOD_MS_ = 2000;
  static constexpr uint8_t BOOTLOADER_EXIT_DELAY_MS_ = 100;
  static constexpr uint32_t FLASH_PAGE_SIZE_ = FLASH_PAGE_SIZE;
  
  static constexpr float TASK_LED_STATUS_FREQ_HZ_ = 20.0f;  // 50ms period

  CommCan* comm_can_;
  CanMessenger* can_messenger_;
  CanMessenger::RxQueueItem current_rx_item_;
  static uint16_t can_receive_size_;
  static bool can_transmit_flag_;
  static BootloaderMain* interrupt_instance_;

  BootloaderState bootloader_state_ = BootloaderState::INIT;
  bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState programming_state_ =
    bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState::PROGRAMMING_STATE_NONE;

  uint32_t last_programmed_address_ = 1;
  bool is_programming_command_received_ = false;
  bluelink::CommandsPayload::ProgrammingCommand received_programming_command_ = {};

  std::unique_ptr<SchedulerOnce> programming_timeout_scheduler_;

  // Enhanced dual LED status with scheduler
  LedStatusHandler* led_status_;
  std::unique_ptr<SchedulerTimer> led_status_scheduler_;
  
  // Timeout schedulers
  std::unique_ptr<SchedulerOnce> boot_timeout_scheduler_;
  std::unique_ptr<SchedulerOnce> force_app_timeout_scheduler_;

  bool is_metadata_send_requested_ = false;
  uint8_t metadata_destination_id_ = 0;
  BootloaderTransport metadata_response_transport_ = BootloaderTransport::CAN;
  bool is_programming_data_send_requested_ = false;
  bluelink::CommandsPayload::ProgrammingCommand programming_data_to_send_;
  uint8_t programming_response_destination_id_ = bluelink::ComponentId::COMPONENT_ID_HLC;
  BootloaderTransport programming_response_transport_ = BootloaderTransport::CAN;
  uint8_t last_programming_source_id_ = bluelink::ComponentId::COMPONENT_ID_HLC;
  BootloaderTransport last_programming_transport_ = BootloaderTransport::CAN;
  bool has_programming_command_transport_ = false;

  BootloaderUsbComm* bootloader_usb_comm_ = nullptr;

  // Double-word programming FSM
  enum class DoubleWordState : uint8_t {
    WAITING_FOR_FIRST_WORD,
    WAITING_FOR_SECOND_WORD
  };
  
  DoubleWordState double_word_state_ = DoubleWordState::WAITING_FOR_FIRST_WORD;
  uint32_t first_word_addr_ = 0;
  uint32_t first_word_data_ = 0;
  uint32_t programming_checksum_ = 0;

 public:
  BootloaderMain(FDCAN_HandleTypeDef* hfdcan, TIM_HandleTypeDef* htim);

  inline bool isInProgrammingFlow() const {
    return (bootloader_state_ == BootloaderState::PROGRAMMING_READY or
            bootloader_state_ == BootloaderState::WAITING_FOR_PROGRAMMING_READY or
            bootloader_state_ == BootloaderState::PROGRAMMING_IN_PROGRESS or
            is_programming_command_received_ or
            isProgrammingCommandMetaDataPresent());
  }

  int run();
  void directEnqueueRxFromInterrupt(const FDCAN_RxHeaderTypeDef& header, const uint8_t* data, uint8_t length);
  bool handleInboundMessage(const BootloaderInboundMessage& message, BootloaderTransport transport);

 private:
  template<typename ModuleType>
  void executeTask(std::unique_ptr<SchedulerTimer>& scheduler, ModuleType* module);
  InterfaceStatus initializeCanInterface();
  InterfaceStatus configureCanFilter();

  void processStateMachine();
  void processRxQueuedMessage();
  void processTxQueue();
  void handlePendingMetadataRequest();
  void handlePendingProgrammingData();
  void handlePendingUsbReplies();
  bool sendCanMessage(uint8_t destination_id, bluelink::PayloadTypeIds payload_type,
                      const void* data, size_t data_size);
  bool sendReply(uint8_t destination_id, bluelink::PayloadTypeIds payload_type, const void* data,
                 size_t data_size, BootloaderTransport transport);
  void handleTimeouts();

  void processProgrammingCommand(const BootloaderInboundMessage& message);
  void processMetaDataRequest(uint8_t source_id, BootloaderTransport transport);

  void transitionToState(BootloaderState new_state);
  void handleInitState();
  void handleWaitingForCommandState();
  void handleProgrammingReadyState();
  void handleWaitingForProgrammingReadyState();
  void handleProgrammingInProgressState();
  void handleProgrammingCompleteState();
  void handleErrorState();
  void handleJumpToAppState();

  void sendProgrammingStartRequests();
  uint32_t handleSingleProgrammingCommand();
  
  // Helper methods for programming command handling
  uint32_t handleProgrammingDone();
  uint32_t handleProgrammingRestart();
  uint32_t handleProgrammingChecksum();
  uint32_t handleDataProgramming(uint32_t address, uint32_t data);
  uint32_t processDoubleWordProgramming(uint32_t address, uint32_t data);
  void sendProgrammingResponse(uint32_t address, uint32_t data);

  bool isValidFlashAddress(uint32_t address) const;
  void prepareFlashForProgramming();

  void jumpToApplication(bool force_jump = false);
  void exitBootloader();
  bool isApplicationPresent() const;
  bool isProgrammingStateFlashed() const;

  void requestMetaDataSend(uint8_t destination_id);
  void requestProgrammingFromHLC();

  bool isProgrammingStartCommand() const;
  bool isProgrammingReadyCommand() const;
  bool isProgrammingDoneAddress(uint32_t address) const;
  bool isProgrammingRestartAddress(uint32_t address) const;
  bool isProgrammingChecksumAddress(uint32_t address) const;
  bool isProgrammingCommandMetaDataPresent() const;

  void resetTimeout();
  void resetToWaitingForCommand();

  uint32_t getFlashAddressForProgrammingState() const;
  uint32_t getFlashSizeForProgrammingState() const;
  void resetProgrammingState();
  void resetDoubleWordProgrammingState();
};

int bootloaderMain(FDCAN_HandleTypeDef* hfdcan, TIM_HandleTypeDef* htim);

#endif /* SRC_BOOTLOADER_MAIN_HPP_ */
