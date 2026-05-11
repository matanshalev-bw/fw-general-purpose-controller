/*
 * comm_interface.cpp
 *
 *  Created on: Jul 23, 2025
 *      Author: ariel
 */

#include "comm_interface.hpp"

///////////////////////////////// SPI  /////////////////////////////////

CommSpi* CommSpi::spi_instance_ = nullptr;

//-------------------- Initialization Methods --------------------

void CommSpi::initializeSpiInstance() {
    spi_instance_ = this;
    resetErrorCount();

    if (isSlaveMode()) {
        initializeSlaveMode();
    }
}

InterfaceStatus CommSpi::initializeSlaveMode() {
    if (handler_ == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    if ((handler_->Instance->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE) {
        __HAL_SPI_ENABLE(handler_);
    }

    spi_state_ = SpiState::IDLE;
    return InterfaceStatus::INTERFACE_OK;
}

//-------------------- Master Mode Methods --------------------

InterfaceStatus CommSpi::read(uint8_t* data, const uint16_t size) {
    if (not isMasterMode() or data == nullptr or size == 0) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    uint8_t oc = opcode_ & (~0x80U);

    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_SPI_Transmit(handler_, &oc, sizeof(opcode_), TIMEOUT_));
    if (status != InterfaceStatus::INTERFACE_OK) {
        return handleSpiError(status);
    }

    status = static_cast<InterfaceStatus>(HAL_SPI_Receive(handler_, data, size, TIMEOUT_));

    if (status == InterfaceStatus::INTERFACE_OK) {
        resetErrorCount();
        return InterfaceStatus::INTERFACE_OK;
    }

    return handleSpiError(status);
}

InterfaceStatus CommSpi::write(const uint8_t* data, const uint16_t size) {
    if (not isMasterMode() or data == nullptr or size == 0) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    uint8_t oc = opcode_ | 0x80U;

    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_SPI_Transmit(handler_, &oc, sizeof(opcode_), TIMEOUT_));
    if (status != InterfaceStatus::INTERFACE_OK) {
        return handleSpiError(status);
    }

    status = static_cast<InterfaceStatus>(HAL_SPI_Transmit(handler_, const_cast<uint8_t*>(data), size, TIMEOUT_));

    if (status == InterfaceStatus::INTERFACE_OK) {
        resetErrorCount();
        return InterfaceStatus::INTERFACE_OK;
    }

    return handleSpiError(status);
}

InterfaceStatus CommSpi::transmitReceive(const uint8_t* tx_data, uint8_t* rx_data, const uint16_t size) {
    if (not isMasterMode() or tx_data == nullptr or rx_data == nullptr or size == 0) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    spi_state_ = SpiState::MASTER_TRANSMITTING;
    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_SPI_TransmitReceive(handler_,
                                                       const_cast<uint8_t*>(tx_data),
                                                       rx_data,
                                                       size,
                                                       TIMEOUT_));
    spi_state_ = (status == InterfaceStatus::INTERFACE_OK) ? SpiState::COMPLETED : SpiState::ERROR;

    if (status == InterfaceStatus::INTERFACE_OK) {
        resetErrorCount();
        return InterfaceStatus::INTERFACE_OK;
    }

    return handleSpiError(status);
}

InterfaceStatus CommSpi::transmitReceiveInterrupt(const uint8_t* tx_data, uint8_t* rx_data, const uint16_t size) {
    if (not isMasterMode() or tx_data == nullptr or rx_data == nullptr or size == 0) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    *transmit_flag_ = false;
    *receive_size_ = 0;
    spi_state_ = SpiState::MASTER_TRANSMITTING;

    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_SPI_TransmitReceive_IT(handler_,
                                                          const_cast<uint8_t*>(tx_data),
                                                          rx_data,
                                                          size));

    if (status == InterfaceStatus::INTERFACE_OK) {
        resetErrorCount();
        return InterfaceStatus::INTERFACE_OK;
    }

    spi_state_ = SpiState::ERROR;
    return handleSpiError(status);
}

//-------------------- Slave Mode Methods --------------------

InterfaceStatus CommSpi::prepareSlaveResponse(const uint8_t* response_data, uint16_t response_size) {
    if (not isSlaveMode() or response_data == nullptr or response_size == 0) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    slave_response_buffer_ = const_cast<uint8_t*>(response_data);
    slave_response_size_ = response_size;

    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus CommSpi::startSlaveListening(uint8_t* receive_buffer, uint16_t buffer_size) {
    if (not isSlaveMode() or receive_buffer == nullptr or buffer_size == 0) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    InterfaceStatus status = validateSlaveBuffers();
    if (status != InterfaceStatus::INTERFACE_OK) {
        return status;
    }

    slave_receive_buffer_ = receive_buffer;
    slave_receive_size_buffer_ = buffer_size;
    *receive_size_ = 0;
    spi_state_ = SpiState::IDLE;

    InterfaceStatus hal_status = static_cast<InterfaceStatus>(HAL_SPI_TransmitReceive_IT(handler_,
                                                              slave_response_buffer_,
                                                              slave_receive_buffer_,
                                                              buffer_size));

    if (hal_status == InterfaceStatus::INTERFACE_OK) {
        spi_state_ = SpiState::SLAVE_RESPONDING;
        resetErrorCount();
        return InterfaceStatus::INTERFACE_OK;
    }

    return handleSpiError(hal_status);
}

InterfaceStatus CommSpi::getSlaveReceivedData(uint8_t* data, uint16_t& received_size) {
    if (not isSlaveMode() or data == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    if (isDataReceived() and slave_receive_buffer_ != nullptr) {
        uint16_t copy_size = (getDataReceivedSize() < slave_receive_size_buffer_) ?
                            getDataReceivedSize() : slave_receive_size_buffer_;

        memcpy(data, slave_receive_buffer_, copy_size);
        received_size = copy_size;

        *receive_size_ = 0;
        prepareSlaveForNextTransaction();

        return InterfaceStatus::INTERFACE_OK;
    }

    received_size = 0;
    return InterfaceStatus::INTERFACE_ERROR;
}

void CommSpi::prepareSlaveForNextTransaction() {
    if (isSlaveMode() and slave_receive_buffer_ != nullptr and slave_response_buffer_ != nullptr) {
        InterfaceStatus status = static_cast<InterfaceStatus>(HAL_SPI_TransmitReceive_IT(handler_,
                                                              slave_response_buffer_,
                                                              slave_receive_buffer_,
                                                              slave_receive_size_buffer_));
        if (status == InterfaceStatus::INTERFACE_OK) {
            spi_state_ = SpiState::SLAVE_RESPONDING;
        } else {
            spi_state_ = SpiState::ERROR;
            incrementErrorCount();
        }
    }
}

InterfaceStatus CommSpi::validateSlaveBuffers() {
    if (slave_response_buffer_ == nullptr or slave_response_size_ == 0) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    return InterfaceStatus::INTERFACE_OK;
}

//-------------------- Common Methods --------------------

InterfaceStatus CommSpi::startReceiveInterrupt(uint8_t* data, const uint16_t size) {
    if (data == nullptr or size == 0) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    if (isMasterMode()) {
        *receive_size_ = 0;
        uint8_t oc = opcode_ & (~0x80U);

        InterfaceStatus status = static_cast<InterfaceStatus>(HAL_SPI_Transmit(handler_, &oc, sizeof(opcode_), TIMEOUT_));
        if (status != InterfaceStatus::INTERFACE_OK) {
            return handleSpiError(status);
        }

        status = static_cast<InterfaceStatus>(HAL_SPI_Receive_IT(handler_, data, size));
        if (status == InterfaceStatus::INTERFACE_OK) {
            *receive_size_ = size;
            resetErrorCount();
            return InterfaceStatus::INTERFACE_OK;
        } else {
            return handleSpiError(status);
        }
    } else {
        return InterfaceStatus::INTERFACE_ERROR;
    }
}

InterfaceStatus CommSpi::startTransmitInterrupt(const uint8_t* data, const uint16_t size) {
    if (data == nullptr or size == 0) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    if (isMasterMode()) {
        *transmit_flag_ = false;
        uint8_t oc = opcode_ | 0x80U;

        InterfaceStatus status = static_cast<InterfaceStatus>(HAL_SPI_Transmit(handler_, &oc, sizeof(opcode_), TIMEOUT_));
        if (status != InterfaceStatus::INTERFACE_OK) {
            return handleSpiError(status);
        }

        status = static_cast<InterfaceStatus>(HAL_SPI_Transmit_IT(handler_, const_cast<uint8_t*>(data), size));
        if (status == InterfaceStatus::INTERFACE_OK) {
            resetErrorCount();
            return InterfaceStatus::INTERFACE_OK;
        } else {
            return handleSpiError(status);
        }
    } else {
        return InterfaceStatus::INTERFACE_ERROR;
    }
}

InterfaceStatus CommSpi::deInit() {
    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_SPI_DeInit(handler_));
    spi_instance_ = nullptr;
    return status;
}

InterfaceStatus CommSpi::setOpcode(const uint8_t opcode) {
    opcode_ = opcode;
    return InterfaceStatus::INTERFACE_OK;
}

//-------------------- Interrupt Management Methods --------------------

void CommSpi::setReceiveComplete(uint16_t size) {
    if (receive_size_) {
        *receive_size_ = size;
    }
    spi_state_ = SpiState::COMPLETED;

    if (not isMasterMode()) {
        prepareSlaveForNextTransaction();
    }
}

void CommSpi::setTransmitComplete() {
    if (transmit_flag_) {
        *transmit_flag_ = true;
    }
    spi_state_ = SpiState::COMPLETED;

}

void CommSpi::handleSpiError() {
    incrementErrorCount();
    spi_state_ = SpiState::ERROR;

}

//-------------------- Error Handling Methods --------------------

InterfaceStatus CommSpi::handleSpiError(InterfaceStatus status) {
    incrementErrorCount();
    last_error_time_ = HAL_GetTick();
    spi_state_ = SpiState::ERROR;

    if (isErrorThresholdExceeded()) {
        // Could trigger error recovery procedure here
    }

    return status;
}

void CommSpi::resetErrorCount() {
    consecutive_error_count_ = 0;
}

void CommSpi::incrementErrorCount() {
    if (consecutive_error_count_ < MAX_CONSECUTIVE_ERRORS_) {
        consecutive_error_count_++;
    }
}

bool CommSpi::isErrorThresholdExceeded() const {
    return consecutive_error_count_ >= MAX_CONSECUTIVE_ERRORS_;
}

///////////////////////////////// CAN  /////////////////////////////////

FDCAN_RxHeaderTypeDef CommCan::interrupt_rx_header_{};
uint8_t CommCan::interrupt_rx_buffer_[CAN_RX_BUFFER_SIZE_] = {0};

CommCan::CanState CommCan::can_state_ = CanState::INIT;
uint8_t CommCan::consecutive_error_count_ = 0;
uint32_t CommCan::last_error_time_ = 0;
CommCan* CommCan::can_instance_ = nullptr;

bool CommCan::tx_fifo_ready_ = true;
uint32_t CommCan::tx_timeout_start_ = 0;

void (*CommCan::tx_complete_callback_)() = nullptr;
void (*CommCan::rx_message_callback_)(const FDCAN_RxHeaderTypeDef&, const uint8_t*, uint8_t) = nullptr;

InterfaceStatus CommCan::initCanPeripheral(bool perform_reset) {
    if (fdcan_handler_ == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    if (perform_reset) {
        __HAL_RCC_FDCAN_FORCE_RESET();
        HAL_Delay(10);
        __HAL_RCC_FDCAN_RELEASE_RESET();

        InterfaceStatus status = static_cast<InterfaceStatus>(HAL_FDCAN_DeInit(fdcan_handler_));
        if (status != InterfaceStatus::INTERFACE_OK) {
            return status;
        }
        
        InterfaceStatus init_status = static_cast<InterfaceStatus>(HAL_FDCAN_Init(fdcan_handler_));
        if (init_status != InterfaceStatus::INTERFACE_OK) {
            return init_status;
        }
    }

    can_state_ = CanState::INIT;
    resetErrorCount();
    clearInterruptData();
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus CommCan::startCanPeripheral() {
    if (fdcan_handler_ == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_FDCAN_Start(fdcan_handler_));
    if (status != InterfaceStatus::INTERFACE_OK) {
        return handleCanError(status);
    }

    can_state_ = CanState::READY;
    resetErrorCount();
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus CommCan::copyInterruptRxData(uint8_t* app_buffer, FDCAN_RxHeaderTypeDef& app_header) {
    if (not isDataReceived() or app_buffer == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    app_header = interrupt_rx_header_;
    memcpy(app_buffer, interrupt_rx_buffer_, CAN_RX_BUFFER_SIZE_);

    clearInterruptData();
    resetErrorCount();

    return InterfaceStatus::INTERFACE_OK;
}

void CommCan::clearInterruptData() {
    if (receive_size_) {
        *receive_size_ = 0;
    }
}

InterfaceStatus CommCan::startTransmitInterrupt(const uint8_t* data, const uint16_t size) {
    if (can_state_ == CanState::ERROR) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    if (data == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_FDCAN_AddMessageToTxFifoQ(fdcan_handler_, &tx_header_, 
                                                    const_cast<uint8_t*>(data)));
    
    if (status == InterfaceStatus::INTERFACE_OK) {
        tx_timeout_start_ = HAL_GetTick();
        can_state_ = CanState::BUSY_TX;
        *transmit_flag_ = false;
        resetErrorCount();
        return InterfaceStatus::INTERFACE_OK;
    }
    else if (status == static_cast<InterfaceStatus>(HAL_FDCAN_ERROR_FIFO_FULL)) {
        return InterfaceStatus::INTERFACE_BUSY;
    }
    
    can_state_ = CanState::ERROR;
    return handleCanError(status);
}

InterfaceStatus CommCan::deInit() {
    can_instance_ = nullptr;

    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_FDCAN_Stop(fdcan_handler_));
    if (status != InterfaceStatus::INTERFACE_OK) {
        return handleCanError(status);
    }

    status = static_cast<InterfaceStatus>(HAL_FDCAN_DeInit(fdcan_handler_));
    can_state_ = CanState::INIT;
    clearInterruptData();

    return status;
}

InterfaceStatus CommCan::configCustomFilter(uint32_t filter_index, CanFilterMode mode, CanFilterScale scale,
                                            uint32_t id1, uint32_t id2, bool use_fifo1) {
    FDCAN_FilterTypeDef filter_config;

    filter_config.IdType = FDCAN_EXTENDED_ID;
    filter_config.FilterIndex = filter_index;
    filter_config.FilterType = static_cast<uint32_t>(mode);
    filter_config.FilterConfig = use_fifo1 ? FDCAN_FILTER_TO_RXFIFO1 : FDCAN_FILTER_TO_RXFIFO0;
    filter_config.FilterID1 = id1;
    filter_config.FilterID2 = id2;

    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_FDCAN_ConfigFilter(fdcan_handler_, &filter_config));

    if (status == InterfaceStatus::INTERFACE_OK) {
        resetErrorCount();
        return InterfaceStatus::INTERFACE_OK;
    }

    return handleCanError(status);
}

InterfaceStatus CommCan::activateNotifications(CanNotificationType notification_type) {
    return activateNotifications(static_cast<uint32_t>(notification_type));
}

InterfaceStatus CommCan::activateNotifications(uint32_t notification_mask) {
    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_FDCAN_ActivateNotification(fdcan_handler_, notification_mask, 0));

    if (status == InterfaceStatus::INTERFACE_OK) {
        resetErrorCount();
        return InterfaceStatus::INTERFACE_OK;
    }

    return handleCanError(status);
}

void CommCan::kickStartTxInterrupts() {
    // FDCAN uses FIFO-based transmission with queue mode
    // The transmission complete callbacks will be triggered automatically
    // when the FIFO becomes empty or when transmission completes
    tx_fifo_ready_ = true;
    if (transmit_flag_) {
        *transmit_flag_ = true;
    }
}

InterfaceStatus CommCan::setTxCanId(const uint32_t& id) {
    if (tx_header_.IdType == static_cast<uint32_t>(CanIdType::CAN_ID_TYPE_STD) and id > 0x7FF) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    if (tx_header_.IdType == static_cast<uint32_t>(CanIdType::CAN_ID_TYPE_EXT) and id > 0x1FFFFFFF) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    tx_header_.Identifier = id;
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus CommCan::setTxDlc(const uint8_t dlc) {
    if (dlc > 8) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    // Convert DLC to FDCAN data length code
    switch (dlc) {
        case 0: tx_header_.DataLength = FDCAN_DLC_BYTES_0; break;
        case 1: tx_header_.DataLength = FDCAN_DLC_BYTES_1; break;
        case 2: tx_header_.DataLength = FDCAN_DLC_BYTES_2; break;
        case 3: tx_header_.DataLength = FDCAN_DLC_BYTES_3; break;
        case 4: tx_header_.DataLength = FDCAN_DLC_BYTES_4; break;
        case 5: tx_header_.DataLength = FDCAN_DLC_BYTES_5; break;
        case 6: tx_header_.DataLength = FDCAN_DLC_BYTES_6; break;
        case 7: tx_header_.DataLength = FDCAN_DLC_BYTES_7; break;
        case 8: tx_header_.DataLength = FDCAN_DLC_BYTES_8; break;
        default: return InterfaceStatus::INTERFACE_ERROR;
    }
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus CommCan::setTxCanIdType(const CanIdType& type) {
    tx_header_.IdType = static_cast<uint32_t>(type);
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus CommCan::setTxRtrType(const CanRtrType& rtr_type) {
    tx_header_.TxFrameType = static_cast<uint32_t>(rtr_type);
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus CommCan::abortAllTransmissions() {
    if (fdcan_handler_ == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    // For G474, abort all Tx FIFO/Queue entries
    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_FDCAN_AbortTxRequest(fdcan_handler_, FDCAN_TX_BUFFER0 | FDCAN_TX_BUFFER1 | FDCAN_TX_BUFFER2));

    if (status == InterfaceStatus::INTERFACE_OK) {
        resetErrorCount();
        return InterfaceStatus::INTERFACE_OK;
    }

    return handleCanError(status);
}

InterfaceStatus CommCan::prepareForBootloader() {
    if (fdcan_handler_ == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    uint32_t notifications = static_cast<uint32_t>(CanNotificationType::TX_FIFO_EMPTY) |
                           static_cast<uint32_t>(CanNotificationType::RX_FIFO0_MSG_PENDING) |
                           static_cast<uint32_t>(CanNotificationType::RX_FIFO1_MSG_PENDING) |
                           static_cast<uint32_t>(CanNotificationType::RX_FIFO0_FULL) |
                           static_cast<uint32_t>(CanNotificationType::RX_FIFO1_FULL) |
                           static_cast<uint32_t>(CanNotificationType::ERROR_WARNING) |
                           static_cast<uint32_t>(CanNotificationType::ERROR_PASSIVE) |
                           static_cast<uint32_t>(CanNotificationType::BUS_OFF) |
                           static_cast<uint32_t>(CanNotificationType::LAST_ERROR_CODE) |
                           static_cast<uint32_t>(CanNotificationType::ERROR);
                           
    InterfaceStatus status = static_cast<InterfaceStatus>(HAL_FDCAN_DeactivateNotification(fdcan_handler_, notifications));
    if (status != InterfaceStatus::INTERFACE_OK) {
        return status;
    }

    status = abortAllTransmissions();
    if (status != InterfaceStatus::INTERFACE_OK) {
        return status;
    }

    return deInit();
}

InterfaceStatus CommCan::handleCanError(InterfaceStatus status) {
    incrementErrorCount();
    last_error_time_ = HAL_GetTick();

    if (isErrorThresholdExceeded()) {
        can_state_ = CanState::ERROR;
    }

    return status;
}

void CommCan::resetErrorCount() {
    consecutive_error_count_ = 0;
}

void CommCan::incrementErrorCount() {
    if (consecutive_error_count_ < MAX_CONSECUTIVE_ERRORS_) {
        consecutive_error_count_++;
    }
}

bool CommCan::isErrorThresholdExceeded() const {
    return consecutive_error_count_ >= MAX_CONSECUTIVE_ERRORS_;
}

CommCan* CommCan::getInstance(FDCAN_HandleTypeDef* handle) {
    (void)handle;
    return can_instance_;
}

InterfaceStatus CommCan::configPayloadTypeFilters(uint32_t destination_id, const uint8_t* payload_types, uint8_t payload_count, bool use_fifo1, uint32_t start_filter_index) {
    uint32_t filter_index = start_filter_index;
    
    for (uint8_t i = 0; i < payload_count; i++) {
        uint8_t payload_type = payload_types[i];
        uint32_t messageId = (destination_id << 8) | payload_type;
        uint32_t messageMask = 0xFFFF;
        
        uint32_t fdcanId = (messageId << 3) | FDCAN_EXTENDED_ID;
        uint32_t fdcanMask = (messageMask << 3) | FDCAN_EXTENDED_ID;

        InterfaceStatus status = configCustomFilter(
            filter_index,
            CanFilterMode::ID_MASK,
            CanFilterScale::SCALE_32BIT,
            fdcanId,
            fdcanMask,
            use_fifo1
        );

        if (status != InterfaceStatus::INTERFACE_OK) {
            return status;
        }
        
        filter_index++;
    }
    
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus CommCan::configHighPriorityFilters(uint32_t destination_id, const uint8_t* payload_types, uint8_t payload_count, uint32_t start_filter_index) {
    return configPayloadTypeFilters(destination_id, payload_types, payload_count, true, start_filter_index);
}

InterfaceStatus CommCan::configDefaultDestinationFilter(uint32_t destination_id, uint32_t filter_index) {
    uint32_t destination_message_id = destination_id << 8;
    uint32_t destination_mask = 0xFF00;
    
    uint32_t fdcanId = (destination_message_id << 3) | FDCAN_EXTENDED_ID;
    uint32_t fdcanMask = (destination_mask << 3) | FDCAN_EXTENDED_ID;

    return configCustomFilter(
        filter_index,
        CanFilterMode::ID_MASK,
        CanFilterScale::SCALE_32BIT,
        fdcanId,
        fdcanMask,
        false
    );
}

void CommCan::handleInterruptRxMessage() {
    if (fdcan_handler_ == nullptr) {
        return;
    }

    // Process both FIFOs - high priority messages go to RxFifo0, low priority to RxFifo1
    processRxFifo(FDCAN_RX_FIFO0);
    processRxFifo(FDCAN_RX_FIFO1);
}

void CommCan::processRxFifo(uint32_t fifo_number) {
    uint32_t fifo_level = HAL_FDCAN_GetRxFifoFillLevel(fdcan_handler_, fifo_number);
    uint8_t messages_processed = 0;
    const uint8_t MAX_INTERRUPT_RX_PROCESS = 3;
    
    while (fifo_level > 0 and messages_processed < MAX_INTERRUPT_RX_PROCESS) {
        InterfaceStatus status = static_cast<InterfaceStatus>(HAL_FDCAN_GetRxMessage(fdcan_handler_, fifo_number,
                                                        &interrupt_rx_header_,
                                                        interrupt_rx_buffer_));

        if (status == InterfaceStatus::INTERFACE_OK) {
            // Convert FDCAN DLC to bytes
            uint8_t dlc_bytes = 0;
            switch (interrupt_rx_header_.DataLength) {
                case FDCAN_DLC_BYTES_0: dlc_bytes = 0; break;
                case FDCAN_DLC_BYTES_1: dlc_bytes = 1; break;
                case FDCAN_DLC_BYTES_2: dlc_bytes = 2; break;
                case FDCAN_DLC_BYTES_3: dlc_bytes = 3; break;
                case FDCAN_DLC_BYTES_4: dlc_bytes = 4; break;
                case FDCAN_DLC_BYTES_5: dlc_bytes = 5; break;
                case FDCAN_DLC_BYTES_6: dlc_bytes = 6; break;
                case FDCAN_DLC_BYTES_7: dlc_bytes = 7; break;
                case FDCAN_DLC_BYTES_8: dlc_bytes = 8; break;
                default: dlc_bytes = 8; break;
            }
            
            if (receive_size_) {
                *receive_size_ = dlc_bytes;
            }
            resetErrorCount();
            
            if (rx_message_callback_ != nullptr) {
                rx_message_callback_(interrupt_rx_header_, interrupt_rx_buffer_, dlc_bytes);
            }
        } else {
            handleCanError(status);
            break;
        }
        
        fifo_level = HAL_FDCAN_GetRxFifoFillLevel(fdcan_handler_, fifo_number);
        messages_processed++;
    }
}



void CommCan::setupTxHeader(FDCAN_TxHeaderTypeDef& tx_header, const bluelink::J1939CanIdStruct& can_id, uint8_t data_size) {
    tx_header.Identifier = CONVERT_CAN_ID_TO_UINT32(can_id);
    tx_header.IdType = FDCAN_EXTENDED_ID;
    tx_header.TxFrameType = FDCAN_DATA_FRAME;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch = FDCAN_BRS_OFF;
    tx_header.FDFormat = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker = 0;
    
    // Convert DLC to FDCAN data length code
    switch (data_size) {
        case 0: tx_header.DataLength = FDCAN_DLC_BYTES_0; break;
        case 1: tx_header.DataLength = FDCAN_DLC_BYTES_1; break;
        case 2: tx_header.DataLength = FDCAN_DLC_BYTES_2; break;
        case 3: tx_header.DataLength = FDCAN_DLC_BYTES_3; break;
        case 4: tx_header.DataLength = FDCAN_DLC_BYTES_4; break;
        case 5: tx_header.DataLength = FDCAN_DLC_BYTES_5; break;
        case 6: tx_header.DataLength = FDCAN_DLC_BYTES_6; break;
        case 7: tx_header.DataLength = FDCAN_DLC_BYTES_7; break;
        case 8: tx_header.DataLength = FDCAN_DLC_BYTES_8; break;
        default: tx_header.DataLength = FDCAN_DLC_BYTES_8; break;
    }
}



extern "C" {
void Error_Handler();

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    CommSpi* spi_instance = CommSpi::getInstance();
    if (spi_instance != nullptr) {
        uint16_t transaction_size = hspi->RxXferSize;
        spi_instance->setReceiveComplete(transaction_size);
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    CommSpi* spi_instance = CommSpi::getInstance();
    if (spi_instance != nullptr) {
        spi_instance->setTransmitComplete();
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    CommSpi* spi_instance = CommSpi::getInstance();
    if (spi_instance != nullptr) {
        uint16_t received_size = hspi->RxXferSize;
        spi_instance->setReceiveComplete(received_size);
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    CommSpi* spi_instance = CommSpi::getInstance();
    if (spi_instance != nullptr) {
        spi_instance->handleSpiError();
    }
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs) {
	CommCan* instance = CommCan::getInstance(hfdcan);
    if (instance != nullptr and (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE)) {
    	instance->handleInterruptRxMessage();
    }
}

void HAL_FDCAN_RxFifo1Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo1ITs) {
	CommCan* instance = CommCan::getInstance(hfdcan);
    if (instance != nullptr and (RxFifo1ITs & FDCAN_IT_RX_FIFO1_NEW_MESSAGE)) {
    	instance->handleInterruptRxMessage();
    }
}

void CommCan::setTransmitComplete() {
	tx_fifo_ready_ = true;
	can_state_ = CanState::READY;
	
	if (transmit_flag_) {
		*transmit_flag_ = true;
	}

	if (tx_complete_callback_ != nullptr) {
		tx_complete_callback_();
	}
}

void HAL_FDCAN_TxFifoEmptyCallback(FDCAN_HandleTypeDef *hfdcan) {
    CommCan* instance = CommCan::getInstance(hfdcan);
    if (instance != nullptr) {
        instance->setTransmitComplete();
    }
}

void HAL_FDCAN_TxBufferCompleteCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t BufferIndexes) {
    (void)BufferIndexes; // Unused parameter
    CommCan* instance = CommCan::getInstance(hfdcan);
    if (instance != nullptr) {
        instance->setTransmitComplete();
    }
}

}
