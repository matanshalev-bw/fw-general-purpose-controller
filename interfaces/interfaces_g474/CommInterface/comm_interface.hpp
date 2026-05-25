/*
 * comm_interface.hpp
 *
 *  Created on: Jul 23, 2025
 *      Author: ariel
 */

#ifndef SRC_F072_COMM_INTERFACE_HPP_
#define SRC_F072_COMM_INTERFACE_HPP_

#include <string.h>

#include "interface_status.hpp"
#include "stm32g4xx_hal.h"
#include "usbd_def.h"
#include "stm32g4xx_hal_fdcan.h"
#include "stm32g4xx_hal_spi.h"
#include "distributed_can_id.hpp"

class CommInterface {
 protected:
  static constexpr uint32_t TIMEOUT_DEFAULT_ = 100U;

  const uint32_t TIMEOUT_;
  uint16_t* receive_size_ = nullptr;
  bool* transmit_flag_ = nullptr;

 public:
  CommInterface(const uint32_t timeout = TIMEOUT_DEFAULT_) : TIMEOUT_(timeout) {}
  CommInterface(uint16_t* receive_size, bool* transmit_flag)
      : TIMEOUT_(TIMEOUT_DEFAULT_), receive_size_(receive_size), transmit_flag_(transmit_flag) {
    if (receive_size == nullptr) {
      static uint16_t no_receive_size;
      receive_size_ = &no_receive_size;
    }
    if (transmit_flag == nullptr) {
      static bool no_transmit_flag;
      transmit_flag_ = &no_transmit_flag;
    }
    *receive_size_ = 0;
    *transmit_flag_ = true;
  }

  inline bool isDataReceived() const { return *receive_size_ > 0; }
  inline bool isTransmitAvailable() const { return *transmit_flag_; }
  inline uint16_t getDataReceivedSize() const { return *receive_size_; }

  virtual InterfaceStatus write(const uint8_t* data, const uint16_t size) { return InterfaceStatus::INTERFACE_ERROR; }
  virtual InterfaceStatus startReceiveInterrupt(uint8_t* data, const uint16_t size) { return InterfaceStatus::INTERFACE_ERROR; }
  virtual InterfaceStatus startTransmitInterrupt(const uint8_t* data, const uint16_t size) { return InterfaceStatus::INTERFACE_ERROR; }
  virtual InterfaceStatus deInit() { return InterfaceStatus::INTERFACE_ERROR; }
};

///////////////////////////////// USB  /////////////////////////////////

class CommUsb : public CommInterface {
  USBD_HandleTypeDef* handler_;

 public:
  CommUsb(USBD_HandleTypeDef* handler, uint16_t* receive_size, bool* transmit_flag)
      : CommInterface(receive_size, transmit_flag), handler_(handler) {}

  InterfaceStatus write(const uint8_t* data, const uint16_t size) override;
  InterfaceStatus startReceiveInterrupt(uint8_t* data, const uint16_t size) override;
  InterfaceStatus startTransmitInterrupt(const uint8_t* data, const uint16_t size) override;
  InterfaceStatus deInit() override;
};

///////////////////////////////// SPI  /////////////////////////////////

class CommSpi : public CommInterface {
 private:
  enum class SpiMode : uint8_t {
    MASTER,
    SLAVE
  };

  enum class SpiState : uint8_t {
    IDLE,
    MASTER_TRANSMITTING,
    SLAVE_RESPONDING,
    COMPLETED,
    ERROR
  };

  static constexpr uint32_t SPI_SLAVE_TIMEOUT_MS_ = 1000U;
  static constexpr uint8_t MAX_CONSECUTIVE_ERRORS_ = 3U;

  SPI_HandleTypeDef* handler_;
  uint8_t opcode_ = 0;

  SpiMode spi_mode_;
  SpiState spi_state_ = SpiState::IDLE;
  uint8_t consecutive_error_count_ = 0;
  uint32_t last_error_time_ = 0;

  static CommSpi* spi_instance_;

  uint8_t* slave_response_buffer_ = nullptr;
  uint16_t slave_response_size_ = 0;
  uint8_t* slave_receive_buffer_ = nullptr;
  uint16_t slave_receive_size_buffer_ = 0;

 public:
  CommSpi(SPI_HandleTypeDef* handler, uint16_t* receive_size, bool* transmit_flag, bool is_slave,
          const uint32_t timeout = TIMEOUT_DEFAULT_)
      : CommInterface(receive_size, transmit_flag), handler_(handler),
        spi_mode_(is_slave ? SpiMode::SLAVE : SpiMode::MASTER) {
    (void)timeout; // timeout kept for API symmetry if needed later
    initializeSpiInstance();
  }

  inline bool isMasterMode() const { return spi_mode_ == SpiMode::MASTER; }
  inline bool isSlaveMode() const { return spi_mode_ == SpiMode::SLAVE; }
  inline SpiState getSpiState() const { return spi_state_; }
  inline uint8_t getConsecutiveErrorCount() const { return consecutive_error_count_; }

  InterfaceStatus read(uint8_t* data, const uint16_t size);
  InterfaceStatus write(const uint8_t* data, const uint16_t size) override;
  InterfaceStatus startReceiveInterrupt(uint8_t* data, const uint16_t size);
  InterfaceStatus startTransmitInterrupt(const uint8_t* data, const uint16_t size) override;
  InterfaceStatus transmitReceive(const uint8_t* tx_data, uint8_t* rx_data, const uint16_t size);
  InterfaceStatus transmitReceiveInterrupt(const uint8_t* tx_data, uint8_t* rx_data, const uint16_t size);

  InterfaceStatus prepareSlaveResponse(const uint8_t* response_data, uint16_t response_size);
  InterfaceStatus startSlaveListening(uint8_t* receive_buffer, uint16_t buffer_size);
  InterfaceStatus getSlaveReceivedData(uint8_t* data, uint16_t& received_size);

  InterfaceStatus deInit() override;
  InterfaceStatus setOpcode(const uint8_t opcode);

  static CommSpi* getInstance() { return spi_instance_; }
  void setReceiveComplete(uint16_t size);
  void setTransmitComplete();
  void handleSpiError();

 private:
  void initializeSpiInstance();
  InterfaceStatus initializeSlaveMode();

  InterfaceStatus handleSpiError(InterfaceStatus status);
  void resetErrorCount();
  void incrementErrorCount();
  bool isErrorThresholdExceeded() const;

  void prepareSlaveForNextTransaction();
  InterfaceStatus validateSlaveBuffers();
};

///////////////////////////////// CAN  /////////////////////////////////

class CommCan : public CommInterface {
 private:
  enum class CanState { INIT, READY, BUSY_TX, ERROR };

  static constexpr uint32_t CAN_TX_TIMEOUT_ = 100U;
  static constexpr uint8_t MAX_CONSECUTIVE_ERRORS_ = 10U;
  static constexpr uint8_t CAN_RX_BUFFER_SIZE_ = 64U;

  FDCAN_HandleTypeDef* fdcan_handler_;
  FDCAN_TxHeaderTypeDef tx_header_{};

  static FDCAN_RxHeaderTypeDef interrupt_rx_header_;
  static uint8_t interrupt_rx_buffer_[CAN_RX_BUFFER_SIZE_];

  static CanState can_state_;
  static uint8_t consecutive_error_count_;
  static uint32_t last_error_time_;
  static CommCan* can_instance_;
  
  static bool tx_fifo_ready_;
  static uint32_t tx_timeout_start_;

 public:
  enum class CanFilterMode : uint32_t {
     ID_MASK = FDCAN_FILTER_MASK,
     ID_LIST = FDCAN_FILTER_DUAL
   };

   enum class CanFilterScale : uint32_t {
     SCALE_16BIT = 0,  // Not applicable for FDCAN
     SCALE_32BIT = 1   // FDCAN always uses 32-bit
   };

   enum class CanNotificationType : uint32_t {
     RX_FIFO0_MSG_PENDING = FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
     RX_FIFO1_MSG_PENDING = FDCAN_IT_RX_FIFO1_NEW_MESSAGE,
	 RX_FIFO0_FULL = FDCAN_IT_RX_FIFO0_FULL,
	 RX_FIFO1_FULL = FDCAN_IT_RX_FIFO1_FULL,
     TX_FIFO_EMPTY = FDCAN_IT_TX_FIFO_EMPTY,
     TX_BUFFER_COMPLETE = FDCAN_IT_TX_COMPLETE,
     ERROR_WARNING = FDCAN_IT_ERROR_WARNING,
     ERROR_PASSIVE = FDCAN_IT_ERROR_PASSIVE,
     BUS_OFF = FDCAN_IT_BUS_OFF,
     LAST_ERROR_CODE = FDCAN_IT_ERROR_LOGGING_OVERFLOW,
     ERROR = FDCAN_IT_ERROR_PASSIVE
   };

  enum CanIdType : uint32_t {
    CAN_ID_TYPE_STD = FDCAN_STANDARD_ID,
    CAN_ID_TYPE_EXT = FDCAN_EXTENDED_ID
  };

  enum CanRtrType : uint32_t {
    CAN_RTR_DATA_TYPE = FDCAN_DATA_FRAME,
    CAN_RTR_REMOTE_TYPE = FDCAN_REMOTE_FRAME
  };

  CommCan(FDCAN_HandleTypeDef* handler, uint16_t* receive_size, bool* transmit_flag, bool perform_reset = true)
      : CommInterface(receive_size, transmit_flag), fdcan_handler_(handler) {
    tx_header_.IdType = CanIdType::CAN_ID_TYPE_STD;
    tx_header_.TxFrameType = CanRtrType::CAN_RTR_DATA_TYPE;
    tx_header_.DataLength = FDCAN_DLC_BYTES_8;
    tx_header_.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header_.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header_.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header_.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx_header_.MessageMarker = 0;

    initCanPeripheral(perform_reset);
    can_instance_ = this;
  }

  inline bool isCanReady() const { return can_state_ == CanState::READY; }
  inline uint8_t getConsecutiveErrorCount() const { return consecutive_error_count_; }
  inline uint32_t getLastErrorTime() const { return last_error_time_; }

  InterfaceStatus startTransmitInterrupt(const uint8_t* data, const uint16_t size) override;
  InterfaceStatus copyInterruptRxData(uint8_t* app_buffer, FDCAN_RxHeaderTypeDef& app_header);
  InterfaceStatus deInit() override;

  InterfaceStatus configCustomFilter(uint32_t filter_index, CanFilterMode mode, CanFilterScale scale,
                                    uint32_t id1, uint32_t id2, bool use_fifo1 = false);


  InterfaceStatus configPayloadTypeFilters(uint32_t destination_id, const uint8_t* payload_types, uint8_t payload_count, bool use_fifo1, uint32_t start_filter_index);
  InterfaceStatus configHighPriorityFilters(uint32_t destination_id, const uint8_t* payload_types, uint8_t payload_count, uint32_t start_filter_index = 0);
  InterfaceStatus configDefaultDestinationFilter(uint32_t destination_id, uint32_t filter_index);
  InterfaceStatus configGlobalFilter();
  InterfaceStatus activateNotifications(CanNotificationType notification_type);
  InterfaceStatus activateNotifications(uint32_t notification_mask);
  void kickStartTxInterrupts();
  InterfaceStatus startCanPeripheral();

  InterfaceStatus setTxCanId(const uint32_t& id);
  InterfaceStatus setTxDlc(const uint8_t dlc);
  InterfaceStatus setTxCanIdType(const CanIdType& type);
  InterfaceStatus setTxRtrType(const CanRtrType& rtr_type);

  InterfaceStatus abortAllTransmissions();
  InterfaceStatus prepareForBootloader();
  
  void setupTxHeader(FDCAN_TxHeaderTypeDef& tx_header, const bluelink::J1939CanIdStruct& can_id, uint8_t data_size);

  static CommCan* getInstance(FDCAN_HandleTypeDef* handle);

  void handleInterruptRxMessage();
  void setTransmitComplete();
  void processRxFifo(uint32_t fifo_number);
  
  static void (*tx_complete_callback_)();
  void setTxCompleteCallback(void (*callback)()) { tx_complete_callback_ = callback; }
  
  static void (*rx_message_callback_)(const FDCAN_RxHeaderTypeDef&, const uint8_t*, uint8_t);
  void setRxMessageCallback(void (*callback)(const FDCAN_RxHeaderTypeDef&, const uint8_t*, uint8_t)) { rx_message_callback_ = callback; }

 private:
  InterfaceStatus initCanPeripheral(bool perform_reset = true);

  InterfaceStatus handleCanError(InterfaceStatus status);
  void resetErrorCount();
  void incrementErrorCount();
  bool isErrorThresholdExceeded() const;

  void clearInterruptData();
};

#endif /* SRC_F072_COMM_INTERFACE_HPP_ */
