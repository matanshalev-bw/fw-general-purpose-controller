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

constexpr float BootloaderMain::TASK_LED_STATUS_FREQ_HZ_;
constexpr uint16_t BootloaderMain::BOOT_TIMEOUT_MS_;
constexpr uint16_t BootloaderMain::FORCE_APP_TIMEOUT_MS_;
constexpr uint16_t BootloaderMain::PROGRAMMING_TIMEOUT_MS_;

extern uint8_t timer_1_flag;

BootloaderMain::BootloaderMain(FDCAN_HandleTypeDef* hfdcan, TIM_HandleTypeDef* htim) {
    comm_can_ = new CommCan(hfdcan, &can_receive_size_, &can_transmit_flag_);
    can_messenger_ = new CanMessenger(comm_can_, bluelink::ComponentId::COMPONENT_ID_BOOTLOADER);
    led_status_ = new LedStatusHandler();
    
    // Initialize schedulers
    led_status_scheduler_ = std::make_unique<SchedulerTimer>(
        htim, TASK_LED_STATUS_FREQ_HZ_, reinterpret_cast<bool*>(&timer_1_flag));
    
    boot_timeout_scheduler_ = std::make_unique<SchedulerOnce>(BOOT_TIMEOUT_MS_);
    force_app_timeout_scheduler_ = std::make_unique<SchedulerOnce>(FORCE_APP_TIMEOUT_MS_);
    programming_timeout_scheduler_ = std::make_unique<SchedulerOnce>(PROGRAMMING_TIMEOUT_MS_);
    
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
                             static_cast<uint32_t>(CommCan::CanNotificationType::RX_FIFO1_MSG_PENDING) |
                             static_cast<uint32_t>(CommCan::CanNotificationType::TX_FIFO_EMPTY);
    
    status = comm_can_->activateNotifications(notifications);
    if (status != InterfaceStatus::INTERFACE_OK) {
        return status;
    }

    comm_can_->setTxCompleteCallback([]() {
        if (interrupt_instance_ != nullptr and interrupt_instance_->can_messenger_) {
            interrupt_instance_->can_messenger_->processQueueFromInterrupt();
        }
    });

    comm_can_->setRxMessageCallback([](const FDCAN_RxHeaderTypeDef& header, const uint8_t* data, uint8_t length) {
        if (interrupt_instance_ != nullptr and interrupt_instance_->can_messenger_) {
            interrupt_instance_->directEnqueueRxFromInterrupt(header, data, length);
        }
    });

    comm_can_->kickStartTxInterrupts();

    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus BootloaderMain::configureCanFilter() {
    uint8_t filter_index = 0;
    
    const uint8_t high_priority_payloads[] = {
        static_cast<uint8_t>(bluelink::PayloadTypeIds::RESET_COMMAND),
        static_cast<uint8_t>(bluelink::PayloadTypeIds::PROGRAMMING_COMMAND),
        static_cast<uint8_t>(bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY)
    };
    
    InterfaceStatus status = comm_can_->configHighPriorityFilters(
        bluelink::ComponentId::COMPONENT_ID_BOOTLOADER,
        high_priority_payloads,
        sizeof(high_priority_payloads) / sizeof(high_priority_payloads[0]),
        filter_index
    );
    
    if (status != InterfaceStatus::INTERFACE_OK) {
        return status;
    }
    
    filter_index += sizeof(high_priority_payloads) / sizeof(high_priority_payloads[0]);
    
    status = comm_can_->configDefaultDestinationFilter(
        bluelink::ComponentId::COMPONENT_ID_BOOTLOADER,
        filter_index
    );
    
    if (status != InterfaceStatus::INTERFACE_OK) {
        return status;
    }
    
    // Configure global filter to reject non-matching standard format messages
    return comm_can_->configGlobalFilter();
}

template<typename ModuleType>
void BootloaderMain::executeTask(std::unique_ptr<SchedulerTimer>& scheduler, ModuleType* module) {
    if (scheduler->isDue()) {
        if (module) {
            module->updateForBootloaderState(bootloader_state_);
        }
        scheduler->restart();
    }
}

int BootloaderMain::run() {
    while (true) {
        processStateMachine();
        handleTimeouts();
        processTxQueue();
        processRxQueuedMessage();
        executeTask(led_status_scheduler_, led_status_);
    }

    return 0;
}

void BootloaderMain::directEnqueueRxFromInterrupt(const FDCAN_RxHeaderTypeDef& header, const uint8_t* data, uint8_t length) {
    const bluelink::J1939CanIdStruct* rx_id =
        reinterpret_cast<const bluelink::J1939CanIdStruct*>(&header.Identifier);

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
        .bootloader_version = {meta.BOOTLOADER_VERSION.major, meta.BOOTLOADER_VERSION.minor},
        .app_version = {meta.APPLICATION_VERSION.major, meta.APPLICATION_VERSION.minor},
        .config_version = {meta.CONFIGURATION_VERSION.major, meta.CONFIGURATION_VERSION.minor},
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
    FDCAN_TxHeaderTypeDef tx_header;
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
    bool is_programming_in_progress = (bootloader_state_ == BootloaderState::WAITING_FOR_PROGRAMMING_READY or
                                        bootloader_state_ == BootloaderState::PROGRAMMING_IN_PROGRESS);
    
    if (is_programming_in_progress and programming_timeout_scheduler_->isDue()) {
        // Dump any pending word before transitioning to error to prevent data loss
        if (double_word_state_ == DoubleWordState::WAITING_FOR_SECOND_WORD) {
            uint64_t double_word = ((uint64_t)0 << 32) | first_word_data_;
            InterfaceStatus status = NonVolatileMemoryInterface::flashProgram(first_word_addr_, double_word);
            if (status == InterfaceStatus::INTERFACE_OK) {
                programming_checksum_ += first_word_data_;
            }
        }
        transitionToState(BootloaderState::ERROR_STATE);
    }

    if (boot_timeout_scheduler_->isDue() and not isInProgrammingFlow()) {
        if (force_app_timeout_scheduler_->isDue()) {
            boot_timeout_scheduler_->restart();
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
    NonVolatileMemoryInterface::updateBootloaderVersionOnMetaData();

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
    programming_timeout_scheduler_->restart();
    // Reset boot timeouts when entering programming flow to prevent premature jump
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
        // Reset last_programmed_address_ for new programming session
        last_programmed_address_ = 1;
		programming_timeout_scheduler_->restart();
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
        programming_timeout_scheduler_->restart();
    }
}

void BootloaderMain::handleProgrammingCompleteState() {
    NonVolatileMemoryInterface::unlockFlash(false);
    NonVolatileMemoryInterface::updateProgrammingStateOnMetaData(ProgrammingState::PROGRAMMING_STATE_FLASHED);
    transitionToState(BootloaderState::JUMP_TO_APP);
}

void BootloaderMain::handleErrorState() {
    // Dump any pending word before resetting to prevent data loss
    if (double_word_state_ == DoubleWordState::WAITING_FOR_SECOND_WORD) {
        uint64_t double_word = ((uint64_t)0 << 32) | first_word_data_;
        InterfaceStatus status = NonVolatileMemoryInterface::flashProgram(first_word_addr_, double_word);
        if (status == InterfaceStatus::INTERFACE_OK) {
            programming_checksum_ += first_word_data_;
        }
    }
    
	NonVolatileMemoryInterface::unlockFlash(false);
    
    resetDoubleWordProgrammingState();
    
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
        return handleProgrammingDone();
    }
    if (isProgrammingRestartAddress(address)) {
        return handleProgrammingRestart();
    }
    if (isProgrammingChecksumAddress(address)) {
        return handleProgrammingChecksum();
    }

    return handleDataProgramming(address, data);
}

uint32_t BootloaderMain::handleProgrammingDone() {
    // Check if we have a pending single word that needs to be programmed
    if (double_word_state_ == DoubleWordState::WAITING_FOR_SECOND_WORD) {
        // Program the single word with padding (second word = 0)
        uint64_t double_word = ((uint64_t)0 << 32) | first_word_data_;
        InterfaceStatus status = NonVolatileMemoryInterface::flashProgram(first_word_addr_, double_word);
        if (status == InterfaceStatus::INTERFACE_OK) {
            programming_checksum_ += first_word_data_;
            double_word_state_ = DoubleWordState::WAITING_FOR_FIRST_WORD;
        }
    }
    
    NonVolatileMemoryInterface::unlockFlash(false);
    sendProgrammingResponse(bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_DONE, programming_checksum_);
    return bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_DONE;
}

uint32_t BootloaderMain::handleProgrammingRestart() {
    resetProgrammingState();
    return bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_START;
}

uint32_t BootloaderMain::handleProgrammingChecksum() {
    sendProgrammingResponse(bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_CHECKSUM, programming_checksum_);
    return bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_CHECKSUM;
}

uint32_t BootloaderMain::handleDataProgramming(uint32_t address, uint32_t data) {
    if (not isValidFlashAddress(address) or address <= last_programmed_address_) {
        return last_programmed_address_;
    }

    return processDoubleWordProgramming(address, data);
}

uint32_t BootloaderMain::processDoubleWordProgramming(uint32_t address, uint32_t data) {
    switch (double_word_state_) {
        case DoubleWordState::WAITING_FOR_FIRST_WORD:
            // Store first word and wait for second
            first_word_addr_ = address;
            first_word_data_ = data;
            double_word_state_ = DoubleWordState::WAITING_FOR_SECOND_WORD;
            sendProgrammingResponse(address, 0);
            return address;
            
        case DoubleWordState::WAITING_FOR_SECOND_WORD:
            if (address == first_word_addr_ + sizeof(uint32_t)) {
                // Sequential addresses - write double-word
                uint64_t double_word = ((uint64_t)data << 32) | first_word_data_;
                InterfaceStatus status = NonVolatileMemoryInterface::flashProgram(first_word_addr_, double_word);
                if (status == InterfaceStatus::INTERFACE_OK) {
                    programming_checksum_ += first_word_data_ + data;
                    double_word_state_ = DoubleWordState::WAITING_FOR_FIRST_WORD;
                    programming_timeout_scheduler_->restart();
                    sendProgrammingResponse(address, 0);
                    return address;
                }
            }
            // Non-sequential or write failed - treat as new first word
            first_word_addr_ = address;
            first_word_data_ = data;
            double_word_state_ = DoubleWordState::WAITING_FOR_SECOND_WORD;
            sendProgrammingResponse(address, 0);
            return address;
            
        default:
            // Invalid state - reset to first word
            double_word_state_ = DoubleWordState::WAITING_FOR_FIRST_WORD;
            return processDoubleWordProgramming(address, data);
    }
}

void BootloaderMain::sendProgrammingResponse(uint32_t address, uint32_t data) {
    bluelink::CommandsPayload::ProgrammingCommand response_cmd;
    response_cmd.programming_command_union.programming_command_data.programming_address = address;
    response_cmd.programming_command_union.programming_command_data.programming_data = data;
    programming_data_to_send_ = response_cmd;
    is_programming_data_send_requested_ = true;
}



bool BootloaderMain::isValidFlashAddress(uint32_t address) const {
    return (address >= FLASH_APPLICATION_ADDRESS) and
           (address < (FLASH_CONFIG_ADDRESS + FLASH_CONFIG_BYTES_SIZE));
}

void BootloaderMain::prepareFlashForProgramming() {
    NonVolatileMemoryInterface::unlockFlash(true);
    
    uint32_t flash_address = getFlashAddressForProgrammingState();
    uint32_t flash_size = getFlashSizeForProgrammingState();
    
    InterfaceStatus status = NonVolatileMemoryInterface::eraseFlash(flash_address, flash_size);
    if (status != InterfaceStatus::INTERFACE_OK) {
        NonVolatileMemoryInterface::unlockFlash(false);
        return;
    }
    
    SystemInterface::delay(FLASH_ERASE_DELAY_MS_);
    
    resetDoubleWordProgrammingState();
}
void BootloaderMain::jumpToApplication(bool force_jump) {
    if ((isApplicationPresent() and isProgrammingStateFlashed()) or force_jump) {

        uint32_t jump_address = *(uint32_t*)(FLASH_APPLICATION_ADDRESS + 4);
        pFunction jump_to_application = (pFunction)jump_address;

        if (jump_address == 0xFFFFFFFF || jump_address == 0x00000000) {
            // Invalid application - stay in bootloader
            resetToWaitingForCommand();
            return;
        }

        exitBootloader();
        __set_MSP(*(uint32_t*)FLASH_APPLICATION_ADDRESS);
        jump_to_application();
    } else {
        // If no valid application, stay in bootloader
        resetToWaitingForCommand();
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
    static SchedulerOnce request_scheduler(REQUEST_PERIOD_MS_);
    
    if (request_scheduler.isDue()) {
        request_scheduler.restart();
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

bool BootloaderMain::isProgrammingChecksumAddress(uint32_t address) const {
    return address == bluelink::CommandsPayload::ProgrammingCommand::PROGRAMMING_STATE_CHECKSUM;
}

bool BootloaderMain::isProgrammingCommandMetaDataPresent() const {
    const volatile MetaData& meta_data = NonVolatileMemoryInterface::META_DATA_;
    return meta_data.programming_state == ProgrammingState::PROGRAMMING_STATE_PROGRAMMING_COMMAND;
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

void BootloaderMain::resetDoubleWordProgrammingState() {
    programming_checksum_ = 0;
    double_word_state_ = DoubleWordState::WAITING_FOR_FIRST_WORD;
    first_word_addr_ = 0;
    first_word_data_ = 0;
}

void BootloaderMain::resetTimeout() {
    boot_timeout_scheduler_->restart();
    force_app_timeout_scheduler_->restart();
}

void BootloaderMain::resetToWaitingForCommand() {
    // Reset programming state to allow new programming commands
    NonVolatileMemoryInterface::updateProgrammingStateOnMetaData(
        ProgrammingState::PROGRAMMING_STATE_WAIT_FOR_PROGRAM);
    // Reset member variables for clean state
    is_programming_command_received_ = false;
    programming_state_ = bluelink::CommandsPayload::ProgrammingCommand::ProgrammingState::PROGRAMMING_STATE_NONE;
    // Reset only boot timeout to prevent immediate re-triggering
    boot_timeout_scheduler_->restart();
    transitionToState(BootloaderState::WAITING_FOR_COMMAND);
}

void BootloaderMain::resetProgrammingState() {
	NonVolatileMemoryInterface::unlockFlash(false);
    programming_timeout_scheduler_->restart();
    
    resetDoubleWordProgrammingState();
    
    NonVolatileMemoryInterface::updateProgrammingStateOnMetaData(
        ProgrammingState::PROGRAMMING_STATE_PROGRAMMING_COMMAND);
}

int bootloaderMain(FDCAN_HandleTypeDef* hfdcan, TIM_HandleTypeDef* htim) {
	BootloaderMain bootloader(hfdcan, htim);
	return bootloader.run();
}

