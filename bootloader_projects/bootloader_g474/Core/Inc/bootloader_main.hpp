/*
 * bootloader_main.hpp
 *
 * Created on: Apr 2, 2025
 * Author: matan
 */

#ifndef SRC_BOOTLOADER_MAIN_HPP_
#define SRC_BOOTLOADER_MAIN_HPP_

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

#define IS_APP_EXIST(app_addr) (((*(uint32_t*) app_addr) & 0x2FFE0000) == 0x20000000)
typedef void (*pFunction)(void);

class BootloaderMain {
public:
  enum class BootloaderState : uint8_t {
    INIT,
    WAITING_FOR_COMMAND,
    PROGRAMMING_READY,
    WAITING_FOR_PROGRAMMING_READY,
    PROGRAMMING_IN_PROGRESS,
    PROGRAMMING_COMPLETE,
    ERROR_STATE,
    JUMP_TO_APP
  };
 private:
  enum class FlashOperationState : uint8_t {
    IDLE,
    UNLOCKING,
    ERASING,
    WRITING,
    LOCKING
  };

  static constexpr uint16_t PROGRAMMING_TIMEOUT_MS_ = 3000;
  static constexpr uint16_t BOOT_TIMEOUT_MS_ = 3000;
  static constexpr uint16_t FORCE_APP_TIMEOUT_MS_ = 5000;
  static constexpr uint16_t FLASH_ERASE_DELAY_MS_ = 1500;
  static constexpr uint16_t REQUEST_PERIOD_MS_ = 2000;
  static constexpr uint8_t BOOTLOADER_EXIT_DELAY_MS_ = 100;
  static constexpr uint32_t FLASH_PAGE_SIZE_ = FLASH_PAGE_SIZE;
  
  static constexpr uint32_t LED_UPDATE_PERIOD_MS_ = 250;
  static constexpr uint32_t LED_BLINK_PERIOD_MS_ = 500;

  CommCan* comm_can_;
  CanMessenger* can_messenger_;
  CanMessenger::RxQueueItem current_rx_item_;
  static uint16_t can_receive_size_;
  static bool can_transmit_flag_;
  static BootloaderMain* interrupt_instance_;

  BootloaderState bootloader_state_ = BootloaderState::INIT;
  FlashOperationState flash_state_ = FlashOperationState::IDLE;
  bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState programming_state_ =
    bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState::PROGRAMMING_STATE_NONE;

  uint32_t last_programmed_address_ = 1;
  bool is_programming_command_received_ = false;
  bluelink::CommandsPayload::ProgrammingCommand received_programming_command_ = {};

  uint32_t programming_timeout_start_ = 0;
  uint32_t boot_start_time_ = 0;

  // Simplified LED state
  uint32_t led_last_update_time_ = 0;
  bool led_state_ = false;
  
  GpioPin green_led_gpio_;

  bool is_metadata_send_requested_ = false;
  uint8_t metadata_destination_id_ = 0;
  bool is_programming_data_send_requested_ = false;
  bluelink::CommandsPayload::ProgrammingCommand programming_data_to_send_;

 public:
  BootloaderMain(CAN_HandleTypeDef* hcan);

  inline bool isProgrammingInProgress() const {
    return bootloader_state_ == BootloaderState::WAITING_FOR_PROGRAMMING_READY or
           bootloader_state_ == BootloaderState::PROGRAMMING_IN_PROGRESS;
  }
  inline bool isBootTimeoutExceeded() const {
    return (HAL_GetTick() - boot_start_time_) > BOOT_TIMEOUT_MS_;
  }
  inline bool isForceAppTimeoutExceeded() const {
    return (HAL_GetTick() - boot_start_time_) > FORCE_APP_TIMEOUT_MS_;
  }
  inline bool isInProgrammingFlow() const {
    return (bootloader_state_ == BootloaderState::PROGRAMMING_READY or
            bootloader_state_ == BootloaderState::WAITING_FOR_PROGRAMMING_READY or
            bootloader_state_ == BootloaderState::PROGRAMMING_IN_PROGRESS or
            is_programming_command_received_ or
            isProgrammingCommandMetaDataPresent());
  }

  int run();
  void directEnqueueRxFromInterrupt(const CAN_RxHeaderTypeDef& header, const uint8_t* data, uint8_t length);

 private:
  InterfaceStatus initializeCanInterface();
  InterfaceStatus configureCanFilter();

  void processStateMachine();
  void processRxQueuedMessage();
  void processTxQueue();
  void handlePendingMetadataRequest();
  void handlePendingProgrammingData();
  bool sendCanMessage(uint8_t destination_id, bluelink::PayloadTypeIds payload_type, const void* data, size_t data_size);
  void handleTimeouts();

  void processProgrammingCommand();
  void processMetaDataRequest();

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

  bool unlockFlash();
  void lockFlash();
  bool eraseFlash();
  bool writeFlashWord(uint32_t address, uint32_t data);
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
  bool isProgrammingCommandMetaDataPresent() const;

  void resetTimeout();
  bool isTimeoutExpired(uint32_t start_time, uint32_t timeout_ms) const;
  uint32_t getFlashAddressForProgrammingState() const;
  uint32_t getFlashSizeForProgrammingState() const;
  void resetProgrammingState();

  // Simplified LED control
  void updateLedPattern();
  inline void setLedState(bool on) {
    GpioInterface::digitalWrite(green_led_gpio_, on ? GpioPinState::PIN_SET : GpioPinState::PIN_RESET);
  }

  inline uint32_t getCurrentTime() const { return HAL_GetTick(); }
};

int bootloaderMain(CAN_HandleTypeDef* hcan);

#endif /* SRC_BOOTLOADER_MAIN_HPP_ */
