/*
 * bootloader_main.cpp
 *
 * Created on: Apr 2, 2025
 * Author: matan
 */

#include "bootloader_main.hpp"

uint16_t BootloaderMain::can_receive_size_ = 0;
bool BootloaderMain::can_transmit_flag_ = true;
BootloaderMain* BootloaderMain::interrupt_instance_ = nullptr;

BootloaderMain::BootloaderMain(CAN_HandleTypeDef* hcan) :
    green_led_gpio_(GpioInterface::createDigitalGpio(GREEN_LED_GPIO_Port, GREEN_LED_Pin)) {
    comm_can_ = new CommCan(hcan, &can_receive_size_, &can_transmit_flag_);
    can_messenger_ = new CanMessenger(comm_can_, bluelink::ComponentId::COMPONENT_ID_BOOTLOADER);
    boot_start_time_ = getCurrentTime();
    interrupt_instance_ = this;

    InterfaceStatus status = initializeCanInterface();
    if (status != InterfaceStatus::INTERFACE_OK) {
        transitionToState(BootloaderState::ERROR_STATE);
    } else {
        transitionToState(BootloaderState::INIT);
    }
}

InterfaceStatus BootloaderMain::initializeCanInterface() {
    InterfaceStatus status = configureCanFilter();
    if (status != InterfaceStatus::INTERFACE_OK) {
        return status;
    }

    status = comm_can_->startCanPeripheral();
    if (status != InterfaceStatus::INTERFACE_OK) {
        return status;
    }

    uint32_t notifications = static_cast<uint32_t>(CommCan::CanNotificationType::RX_FIFO0_MSG_PENDING) |
                             static_cast<uint32_t>(CommCan::CanNotificationType::TX_MAILBOX_EMPTY);
    
    status = comm_can_->activateNotifications(notifications);
    if (status != InterfaceStatus::INTERFACE_OK) {
        return status;
    }

    comm_can_->setTxCompleteCallback([]() {
        if (interrupt_instance_ != nullptr and interrupt_instance_->can_messenger_) {
            interrupt_instance_->can_messenger_->processQueueFromInterrupt();
        }
    });

    comm_can_->setRxMessageCallback([](const CAN_RxHeaderTypeDef& header, const uint8_t* data, uint8_t length) {
        if (interrupt_instance_ != nullptr and interrupt_instance_->can_messenger_) {
            interrupt_instance_->directEnqueueRxFromInterrupt(header, data, length);
        }
    });

    comm_can_->kickStartTxInterrupts();

    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus BootloaderMain::configureCanFilter() {
    uint32_t destinationIdValue = bluelink::ComponentId::COMPONENT_ID_BOOTLOADER;
    uint32_t destinationIdMask = 0xFF;

    uint32_t idToMatch = (destinationIdValue << 8);
    uint32_t idMask = (destinationIdMask << 8);

    uint32_t filterIdHigh = (idToMatch >> 13) & 0xFFFF;
    uint32_t filterIdLow = ((idToMatch << 3) & 0xFFF8) | CAN_ID_EXT;
    uint32_t filterMaskHigh = (idMask >> 13) & 0xFFFF;
    uint32_t filterMaskLow = ((idMask << 3) & 0xFFF8) | CAN_ID_EXT;

    return comm_can_->configCustomFilter(
        0,
        CommCan::CanFilterMode::ID_MASK,
        CommCan::CanFilterScale::SCALE_32BIT,
        filterIdHigh,
        filterIdLow,
        filterMaskHigh,
        filterMaskLow
    );
}

int BootloaderMain::run() {
    while (true) {
        processStateMachine();
        handleTimeouts();
        processTxQueue();
        processRxQueuedMessage();
        updateLedPattern();
    }

    return 0;
}

void BootloaderMain::directEnqueueRxFromInterrupt(const CAN_RxHeaderTypeDef& header, const uint8_t* data, uint8_t length) {
    const bluelink::J1939CanIdStruct* rx_id =
        reinterpret_cast<const bluelink::J1939CanIdStruct*>(&header.ExtId);

    if (rx_id->destination_id != bluelink::COMPONENT_ID_BOOTLOADER) {
        return;
    }

    if (can_messenger_) {
        can_messenger_->enqueueRxMessageFromInterrupt(header, data, length);
    }
}

void BootloaderMain::processStateMachine() {
    switch (bootloader_state_) {
        case BootloaderState::INIT:
            handleInitState();
            break;
        case BootloaderState::WAITING_FOR_COMMAND:
            handleWaitingForCommandState();
            break;
        case BootloaderState::PROGRAMMING_READY:
            handleProgrammingReadyState();
            break;
        case BootloaderState::WAITING_FOR_PROGRAMMING_READY:
            handleWaitingForProgrammingReadyState();
            break;
        case BootloaderState::PROGRAMMING_IN_PROGRESS:
            handleProgrammingInProgressState();
            break;
        case BootloaderState::PROGRAMMING_COMPLETE:
            handleProgrammingCompleteState();
            break;
        case BootloaderState::ERROR_STATE:
            handleErrorState();
            break;
        case BootloaderState::JUMP_TO_APP:
            handleJumpToAppState();
            break;
        default:
            transitionToState(BootloaderState::ERROR_STATE);
            break;
    }
}
void BootloaderMain::processRxQueuedMessage() {
    if (not can_messenger_ or can_messenger_->isRxQueueEmpty()) {
        return;
    }

    uint8_t messages_processed = 0;
    const uint8_t MAX_RX_MESSAGES_PER_TICK = CanMessenger::MAX_RX_PROCESS_PER_TICK_;

    while (messages_processed < MAX_RX_MESSAGES_PER_TICK and
           can_messenger_->getNextRxMessage(current_rx_item_)) {

        const bluelink::PayloadTypeIds payload_type = static_cast<bluelink::PayloadTypeIds>(current_rx_item_.payload_type_id);

        switch (payload_type) {
        case bluelink::PayloadTypeIds::PROGRAMMING_COMMAND:
            processProgrammingCommand();
            break;

        case bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY:
            processMetaDataRequest();
            break;

        default:
            break;
        }

        messages_processed++;
    }
}

void BootloaderMain::processTxQueue() {
    can_messenger_->processQueueFromTick();
    handlePendingMetadataRequest();
    handlePendingProgrammingData();
}

void BootloaderMain::handlePendingMetadataRequest() {
    if (not is_metadata_send_requested_) return;

    static const auto& meta = NonVolatileMemoryInterface::META_DATA_;
    bluelink::TelemetryPayload::ControllerMetaData data = {
        .component_id = meta.MY_COMPONENT_ID,
        .app_version = {meta.APPLICATION_VERSION.major, meta.APPLICATION_VERSION.minor, meta.APPLICATION_VERSION.patch},
        .config_version = {meta.CONFIGURATION_VERSION.major, meta.CONFIGURATION_VERSION.minor, meta.CONFIGURATION_VERSION.patch},
        .config_type = meta.config_type.type
    };

    if (sendCanMessage(metadata_destination_id_, bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY,
                       &data, sizeof(data))) {
        is_metadata_send_requested_ = false;
    }
}

void BootloaderMain::handlePendingProgrammingData() {
    if (not is_programming_data_send_requested_) return;

    if (sendCanMessage(bluelink::ComponentId::COMPONENT_ID_HLC, bluelink::PayloadTypeIds::PROGRAMMING_COMMAND,
                       &programming_data_to_send_, sizeof(programming_data_to_send_))) {
        is_programming_data_send_requested_ = false;
    }
}

bool BootloaderMain::sendCanMessage(uint8_t destination_id, bluelink::PayloadTypeIds payload_type,
                                    const void* data, size_t data_size) {
    CAN_TxHeaderTypeDef tx_header;
    bluelink::J1939CanIdStruct can_id(
        bluelink::ComponentId::COMPONENT_ID_BOOTLOADER,
        destination_id,
        payload_type
    );
    comm_can_->setupTxHeader(tx_header, can_id, data_size);

    return can_messenger_->enqueueTxMessage(tx_header,
                                           reinterpret_cast<const uint8_t*>(data),
                                           data_size);
}

void BootloaderMain::handleTimeouts() {
    if (isProgrammingInProgress() and
        isTimeoutExpired(programming_timeout_start_, PROGRAMMING_TIMEOUT_MS_)) {

        transitionToState(BootloaderState::ERROR_STATE);
    }

    if (isBootTimeoutExceeded() and not isInProgrammingFlow()) {
        if (isForceAppTimeoutExceeded()) {
            boot_start_time_ = getCurrentTime();
            transitionToState(BootloaderState::JUMP_TO_APP);
        } else {
            jumpToApplication(false);
            requestProgrammingFromHLC();
        }
    }
}

void BootloaderMain::processProgrammingCommand() {
    memcpy(&received_programming_command_, current_rx_item_.data, sizeof(received_programming_command_));
    
    auto cmd_type = received_programming_command_.programming_command_union.programming_command_type;
    
    if (bootloader_state_ == BootloaderState::WAITING_FOR_COMMAND and 
        cmd_type == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_START) {
        transitionToState(BootloaderState::PROGRAMMING_READY);
        return;
    }
    
    bool is_state_cmd = (cmd_type == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_START) or
                       (cmd_type == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_APPLICATION_FLASHING) or
                       (cmd_type == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_CONFIG_FLASHING);
    
    if (is_state_cmd and bootloader_state_ != BootloaderState::WAITING_FOR_COMMAND and 
        bootloader_state_ != BootloaderState::WAITING_FOR_PROGRAMMING_READY) {
        return;
    }
    
    is_programming_command_received_ = true;
}

void BootloaderMain::processMetaDataRequest() {
    requestMetaDataSend(current_rx_item_.source_id);
}
void BootloaderMain::transitionToState(BootloaderState new_state) {
    bootloader_state_ = new_state;
}

void BootloaderMain::handleInitState() {
    if (isProgrammingCommandMetaDataPresent()) {
        transitionToState(BootloaderState::PROGRAMMING_READY);
    } else {
        transitionToState(BootloaderState::WAITING_FOR_COMMAND);
    }
}

void BootloaderMain::handleWaitingForCommandState() {
    if (isProgrammingStartCommand()) {
        is_programming_command_received_ = false;
        transitionToState(BootloaderState::PROGRAMMING_READY);
    }
}

void BootloaderMain::handleProgrammingReadyState() {
    sendProgrammingStartRequests();
    resetTimeout();
    transitionToState(BootloaderState::WAITING_FOR_PROGRAMMING_READY);
}

void BootloaderMain::handleWaitingForProgrammingReadyState() {
    if (isProgrammingReadyCommand()) {
        is_programming_command_received_ = false;
        
        programming_state_ = received_programming_command_.programming_command_union.programming_command_type;
        
        NonVolatileMemoryInterface::updateProgrammingStateOnMetaData(
            ProgrammingState::PROGRAMMING_STATE_IN_PROGRESS);
        prepareFlashForProgramming();
        resetTimeout();
        transitionToState(BootloaderState::PROGRAMMING_IN_PROGRESS);
    }
}

void BootloaderMain::handleProgrammingInProgressState() {
    if (is_programming_command_received_) {
        is_programming_command_received_ = false;
        last_programmed_address_ = handleSingleProgrammingCommand();
        
        if (isProgrammingDoneAddress(last_programmed_address_)) {
            transitionToState(BootloaderState::PROGRAMMING_COMPLETE);
        } else if (isProgrammingRestartAddress(last_programmed_address_)) {
            resetProgrammingState();
            transitionToState(BootloaderState::INIT);
        }
        resetTimeout();
    }
}

void BootloaderMain::handleProgrammingCompleteState() {
    lockFlash();
    transitionToState(BootloaderState::JUMP_TO_APP);
}

void BootloaderMain::handleErrorState() {
	lockFlash();
    
    ProgrammingState new_state = (programming_state_ == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_APPLICATION_FLASHING or
                                  programming_state_ == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_CONFIG_FLASHING) ?
                                 ProgrammingState::PROGRAMMING_STATE_WAIT_FOR_PROGRAM :
                                 ProgrammingState::PROGRAMMING_STATE_FLASHED;
    
    NonVolatileMemoryInterface::updateProgrammingStateOnMetaData(new_state);
    SystemInterface::delay(BOOTLOADER_EXIT_DELAY_MS_);
    transitionToState(BootloaderState::JUMP_TO_APP);
}

void BootloaderMain::handleJumpToAppState() {
    jumpToApplication(true);
}

void BootloaderMain::sendProgrammingStartRequests() {
    bluelink::CommandsPayload::ProgrammingCommand prog_cmd;
    prog_cmd.programming_command_union.programming_command_type =
        bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_START;

    programming_data_to_send_ = prog_cmd;
    is_programming_data_send_requested_ = true;
}

uint32_t BootloaderMain::handleSingleProgrammingCommand() {
    const uint32_t address = received_programming_command_.programming_command_union.programming_command_data.programming_address;
    const uint32_t data = received_programming_command_.programming_command_union.programming_command_data.programming_data;

    if (isProgrammingDoneAddress(address)) {
        return bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_DONE;
    }

    if (isProgrammingRestartAddress(address)) {
        resetProgrammingState();
        return bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_START;
    }

    bool write_success = false;
    uint32_t new_address = last_programmed_address_;

    if (isValidFlashAddress(address) and address > last_programmed_address_) {
        write_success = writeFlashWord(address, data);
        if (write_success) {
            new_address = address;
            resetTimeout();
        }
    }

    bluelink::CommandsPayload::ProgrammingCommand response_cmd;
    response_cmd.programming_command_union.programming_command_data.programming_address = new_address;
    response_cmd.programming_command_union.programming_command_data.programming_data = 0;
    programming_data_to_send_ = response_cmd;
    is_programming_data_send_requested_ = true;

    return new_address;
}
bool BootloaderMain::unlockFlash() {
    flash_state_ = FlashOperationState::UNLOCKING;
    HAL_StatusTypeDef status = HAL_FLASH_Unlock();

    if (status == HAL_OK) {
        flash_state_ = FlashOperationState::IDLE;
        return true;
    }

    flash_state_ = FlashOperationState::IDLE;
    return false;
}

void BootloaderMain::lockFlash() {
    flash_state_ = FlashOperationState::LOCKING;
    HAL_FLASH_Lock();
    flash_state_ = FlashOperationState::IDLE;
}

bool BootloaderMain::eraseFlash() {
    flash_state_ = FlashOperationState::ERASING;

    uint32_t flash_address = getFlashAddressForProgrammingState();
    uint32_t flash_size = getFlashSizeForProgrammingState();

    const uint32_t nbPages = (flash_size + FLASH_PAGE_SIZE_ - 1) / FLASH_PAGE_SIZE_;

    FLASH_EraseInitTypeDef EraseInitStruct = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .PageAddress = flash_address,
        .NbPages = nbPages
    };

    uint32_t page_error;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&EraseInitStruct, &page_error);

    flash_state_ = FlashOperationState::IDLE;

    if (status != HAL_OK) {
        lockFlash();
        return false;
    }

    return true;
}

bool BootloaderMain::writeFlashWord(uint32_t address, uint32_t data) {
    flash_state_ = FlashOperationState::WRITING;

    HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data);

    flash_state_ = FlashOperationState::IDLE;

    return (status == HAL_OK);
}

bool BootloaderMain::isValidFlashAddress(uint32_t address) const {
    return (address >= FLASH_APPLICATION_ADDRESS) and
           (address < (FLASH_CONFIG_ADDRESS + FLASH_CONFIG_BYTES_SIZE));
}

void BootloaderMain::prepareFlashForProgramming() {
    unlockFlash();
    eraseFlash();
    SystemInterface::delay(FLASH_ERASE_DELAY_MS_);
}
void BootloaderMain::jumpToApplication(bool force_jump) {
    if ((isApplicationPresent() and isProgrammingStateFlashed()) or force_jump) {

        uint32_t jump_address = *(uint32_t*)(FLASH_APPLICATION_ADDRESS + 4);
        pFunction jump_to_application = (pFunction)jump_address;

        if (jump_address == 0xFFFFFFFF || jump_address == 0x00000000) {
            // Invalid application - stay in bootloader or reset
            transitionToState(BootloaderState::INIT);
            return;
        }

        exitBootloader();
        __set_MSP(*(uint32_t*)FLASH_APPLICATION_ADDRESS);
        jump_to_application();
    }
}

void BootloaderMain::exitBootloader() {
    comm_can_->prepareForBootloader();
    SystemInterface::delay(BOOTLOADER_EXIT_DELAY_MS_);
}

bool BootloaderMain::isApplicationPresent() const {
    return IS_APP_EXIST(FLASH_APPLICATION_ADDRESS);
}

bool BootloaderMain::isProgrammingStateFlashed() const {
    const volatile MetaData& meta_data = NonVolatileMemoryInterface::META_DATA_;
    return meta_data.programming_state == ProgrammingState::PROGRAMMING_STATE_FLASHED;
}



void BootloaderMain::requestMetaDataSend(uint8_t destination_id) {
    metadata_destination_id_ = destination_id;
    is_metadata_send_requested_ = true;
}

void BootloaderMain::requestProgrammingFromHLC() {
    static uint32_t last_request_time = 0;
    
    if (isTimeoutExpired(last_request_time, REQUEST_PERIOD_MS_)) {
        last_request_time = getCurrentTime();
        requestMetaDataSend(bluelink::ComponentId::COMPONENT_ID_HLC);
    }
}

bool BootloaderMain::isProgrammingStartCommand() const {
    const bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState start_state =
        bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_START;

    return is_programming_command_received_ and
           (received_programming_command_.programming_command_union.programming_command_type == start_state);
}

bool BootloaderMain::isProgrammingReadyCommand() const {
    const bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState app_flashing =
        bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_APPLICATION_FLASHING;
    const bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState config_flashing =
        bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_CONFIG_FLASHING;

    return is_programming_command_received_ and
           ((received_programming_command_.programming_command_union.programming_command_type == app_flashing) or
            (received_programming_command_.programming_command_union.programming_command_type == config_flashing));
}

bool BootloaderMain::isProgrammingDoneAddress(uint32_t address) const {
    return address == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_DONE;
}

bool BootloaderMain::isProgrammingRestartAddress(uint32_t address) const {
    return address == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_START;
}

bool BootloaderMain::isProgrammingCommandMetaDataPresent() const {
    const volatile MetaData& meta_data = NonVolatileMemoryInterface::META_DATA_;
    return meta_data.programming_state == ProgrammingState::PROGRAMMING_STATE_PROGRAMMING_COMMAND;
}
void BootloaderMain::resetTimeout() {
    programming_timeout_start_ = getCurrentTime();
}

bool BootloaderMain::isTimeoutExpired(uint32_t start_time, uint32_t timeout_ms) const {
    return (getCurrentTime() - start_time) > timeout_ms;
}

uint32_t BootloaderMain::getFlashAddressForProgrammingState() const {
    if (programming_state_ == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_APPLICATION_FLASHING) {
        return FLASH_APPLICATION_ADDRESS;
    } else if (programming_state_ == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_CONFIG_FLASHING) {
        return FLASH_CONFIG_ADDRESS;
    } else {
        return FLASH_CONFIG_ADDRESS;
    }
}

uint32_t BootloaderMain::getFlashSizeForProgrammingState() const {
    if (programming_state_ == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_APPLICATION_FLASHING) {
        return FLASH_APPLICATION_BYTES_SIZE;
    } else if (programming_state_ == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_CONFIG_FLASHING) {
        return FLASH_CONFIG_BYTES_SIZE;
    } else {
        // Default to config size for safety, but this shouldn't happen
        return FLASH_CONFIG_BYTES_SIZE;
    }
}

void BootloaderMain::resetProgrammingState() {
	lockFlash();
    resetTimeout();
    NonVolatileMemoryInterface::updateProgrammingStateOnMetaData(
        ProgrammingState::PROGRAMMING_STATE_PROGRAMMING_COMMAND);
}

void BootloaderMain::updateLedPattern() {
    uint32_t current_time = getCurrentTime();
    
    if (current_time - led_last_update_time_ < LED_UPDATE_PERIOD_MS_) {
        return;
    }
    led_last_update_time_ = current_time;
    
    switch (bootloader_state_) {
        case BootloaderState::INIT:
        case BootloaderState::WAITING_FOR_COMMAND:
            // Slow blink for normal operation
            led_state_ = (current_time % LED_BLINK_PERIOD_MS_) < (LED_BLINK_PERIOD_MS_ / 2);
            break;
            
        case BootloaderState::PROGRAMMING_READY:
        case BootloaderState::WAITING_FOR_PROGRAMMING_READY:
        case BootloaderState::PROGRAMMING_IN_PROGRESS:
            // Solid ON during programming
            led_state_ = true;
            break;
            
        case BootloaderState::PROGRAMMING_COMPLETE:
            // Solid ON for success
            led_state_ = true;
            break;
            
        case BootloaderState::ERROR_STATE:
            // Fast blink for error
            led_state_ = (current_time % (LED_BLINK_PERIOD_MS_ / 4)) < (LED_BLINK_PERIOD_MS_ / 8);
            break;
            
        case BootloaderState::JUMP_TO_APP:
            // OFF before jump
            led_state_ = false;
            break;
    }
    
    setLedState(led_state_);
}

int bootloaderMain(CAN_HandleTypeDef* hcan) {
	BootloaderMain bootloader(hcan);
	return bootloader.run();
}
