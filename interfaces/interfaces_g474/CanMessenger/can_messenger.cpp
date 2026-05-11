/*
 * can_messenger.cpp
 *
 * Created on: Jan 2, 2025
 * Author: ariel
 */

#include "can_messenger.hpp"
#include "comm_interface.hpp"

CanMessenger::TxQueueItem CanMessenger::tx_queue_[TX_QUEUE_SIZE_];
volatile uint8_t CanMessenger::tx_queue_head_ = 0;
volatile uint8_t CanMessenger::tx_queue_tail_ = 0;
volatile uint8_t CanMessenger::tx_queue_count_ = 0;

CanMessenger::RxQueueItem CanMessenger::rx_queue_[RX_QUEUE_SIZE_];
volatile uint8_t CanMessenger::rx_queue_head_ = 0;
volatile uint8_t CanMessenger::rx_queue_tail_ = 0;
volatile uint8_t CanMessenger::rx_queue_count_ = 0;

CanMessenger::Statistics CanMessenger::stats_;

CanMessenger::CanMessenger(CommCan* comm_can, bluelink::ComponentId source_component_id)
    : comm_can_(comm_can), source_component_id_(source_component_id) {
    
    for (uint8_t i = 0; i < TX_QUEUE_SIZE_; i++) {
        tx_queue_[i] = TxQueueItem();
    }
    
    for (uint8_t i = 0; i < RX_QUEUE_SIZE_; i++) {
        rx_queue_[i] = RxQueueItem();
    }
    
    resetStatistics();
}

bool CanMessenger::enqueueTxMessage(const FDCAN_TxHeaderTypeDef& tx_header, const uint8_t* data, uint8_t length) {
    if (data == nullptr or length == 0 or length > 64) {
        return false;
    }
    
    const bluelink::J1939CanIdStruct* can_id = 
        reinterpret_cast<const bluelink::J1939CanIdStruct*>(&tx_header.Identifier);
    
    enterCriticalSection();
    
    if (isTxQueueFull()) {
        stats_.tx_queue_full_drops++;
        exitCriticalSection();
        return false;
    }
    
    TxQueueItem new_item;
    new_item.priority = determineTxPriority(can_id->payload_type);
    new_item.payload_type_id = can_id->payload_type;  
    new_item.destination_id = can_id->destination_id;
    new_item.length = length;
    new_item.timestamp = HAL_GetTick();
    new_item.pending = true;
    memcpy(new_item.data, data, length);
    
    insertTxWithPriority(new_item);
    
    stats_.tx_messages_enqueued++;
    
    exitCriticalSection();
    return true;
}

bool CanMessenger::enqueueRxMessage(const FDCAN_RxHeaderTypeDef& rx_header, const uint8_t* data, uint8_t length) {
    if (data == nullptr or length == 0 or length > 64) {
        return false;
    }
    
    const bluelink::J1939CanIdStruct* can_id = 
        reinterpret_cast<const bluelink::J1939CanIdStruct*>(&rx_header.Identifier);
    
    enterCriticalSection();
    
    if (isRxQueueFull()) {
        stats_.rx_queue_full_drops++;
        exitCriticalSection();
        return false;
    }
    
    RxQueueItem new_item;
    new_item.priority = determineRxPriority(can_id->payload_type);
    new_item.payload_type_id = can_id->payload_type;
    new_item.source_id = can_id->source_id;
    new_item.destination_id = can_id->destination_id;
    new_item.length = length;
    new_item.timestamp = HAL_GetTick();
    new_item.can_id = rx_header.Identifier;
    new_item.pending = true;
    memcpy(new_item.data, data, length);
    
    insertRxWithPriority(new_item);
    
    stats_.rx_messages_enqueued++;
    
    exitCriticalSection();
    return true;
}

bool CanMessenger::enqueueRxMessageFromInterrupt(const FDCAN_RxHeaderTypeDef& rx_header, const uint8_t* data, uint8_t length) {
    // Optimized for interrupt context - minimal processing, no complex validation
    if (data == nullptr or length == 0 or length > 64) {
        return false;
    }
    
    // Quick queue full check without entering critical section initially
    if (isRxQueueFull()) {
        stats_.rx_queue_full_drops++;
        return false;
    }
    
    const bluelink::J1939CanIdStruct* can_id = 
        reinterpret_cast<const bluelink::J1939CanIdStruct*>(&rx_header.Identifier);
    
    // Critical section should be as short as possible in interrupt context
    enterCriticalSection();
    
    // Double-check queue full condition inside critical section
    if (isRxQueueFull()) {
        stats_.rx_queue_full_drops++;
        exitCriticalSection();
        return false;
    }
    
    // Fast enqueue - pre-calculate values to minimize time in critical section
    RxQueueItem new_item;
    new_item.priority = determineRxPriority(can_id->payload_type);
    new_item.payload_type_id = can_id->payload_type;
    new_item.source_id = can_id->source_id;
    new_item.destination_id = can_id->destination_id;
    new_item.length = length;
    new_item.timestamp = HAL_GetTick();
    new_item.can_id = rx_header.Identifier;
    new_item.pending = true;
    
    // Fast memory copy - unrolled for common lengths
    if (length <= 4) {
        uint32_t* src32 = (uint32_t*)data;
        uint32_t* dst32 = (uint32_t*)new_item.data;
        *dst32 = *src32;
    } else {
        memcpy(new_item.data, data, length);
    }
    
    insertRxWithPriority(new_item);
    stats_.rx_messages_enqueued++;
    
    exitCriticalSection();
    return true;
}

bool CanMessenger::getNextRxMessage(RxQueueItem& item) {
    enterCriticalSection();
    bool dequeued = dequeueRxMessage(item);
    exitCriticalSection();
    
    if (dequeued) {
        stats_.rx_messages_processed++;
        stats_.rx_tick_processes++;
    }
    
    return dequeued;
}

void CanMessenger::processQueueFromTick() {
    const uint8_t MAX_TICK_MESSAGES = 3;
    uint8_t messages_processed = 0;
    
    if (comm_can_ == nullptr) {
        return;
    }

    while (not isTxQueueEmpty() and messages_processed < MAX_TICK_MESSAGES) {
        TxQueueItem item;
        
        enterCriticalSection();
        bool dequeued = dequeueTxMessage(item);
        exitCriticalSection();
        
        if (not dequeued) {
            break;
        }
        
        InterfaceStatus result = sendQueuedMessage(item);
        
        if (result == InterfaceStatus::INTERFACE_BUSY) {
            TxPriority retry_priority = static_cast<TxPriority>(
                static_cast<uint8_t>(item.priority) + 1
            );
                if (retry_priority > TxPriority::ANALOG_TELEMETRY) {
        retry_priority = TxPriority::ANALOG_TELEMETRY;
            }
            
            TxQueueItem retry_item = item;
            retry_item.priority = retry_priority;
            insertTxWithPriority(retry_item);
            stats_.tx_busy_retries++;
            break;
        } else if (result == InterfaceStatus::INTERFACE_OK) {
            stats_.tx_messages_sent++;
            stats_.tx_tick_sends++;
        }
        
        messages_processed++;
    }
}

void CanMessenger::processQueueFromInterrupt() {
    uint8_t messages_sent = 0;
    
    while (not isTxQueueEmpty() and messages_sent < MAX_INTERRUPT_SENDS_) {
        TxQueueItem item;
        
        if (not dequeueTxMessage(item)) {
            break;
        }
        
        InterfaceStatus result = sendQueuedMessage(item);
        
        if (result == InterfaceStatus::INTERFACE_BUSY) {
            insertTxWithPriority(item);
            break;
        } else if (result == InterfaceStatus::INTERFACE_OK) {
            stats_.tx_messages_sent++;
            stats_.tx_interrupt_sends++;
            messages_sent++;
        }
    }
}

bool CanMessenger::processRxQueueFromTick() {
    if (isRxQueueEmpty()) {
        return false;
    }
    
    RxQueueItem item;
    
    enterCriticalSection();
    bool dequeued = dequeueRxMessage(item);
    exitCriticalSection();
    
    if (not dequeued) {
        return false;
    }
    
    stats_.rx_messages_processed++;
    stats_.rx_tick_processes++;
    
    return true;
}

void CanMessenger::insertTxWithPriority(const TxQueueItem& item) {
    uint8_t insert_pos = tx_queue_tail_;
    
    if (tx_queue_count_ > 0) {
        for (uint8_t i = 0; i < tx_queue_count_; i++) {
            uint8_t check_pos = (tx_queue_head_ + i) % TX_QUEUE_SIZE_;
            if (tx_queue_[check_pos].priority > item.priority) {
                insert_pos = check_pos;
                
                uint8_t shift_count = tx_queue_count_ - i;
                for (uint8_t j = 0; j < shift_count; j++) {
                    uint8_t from_pos = (tx_queue_tail_ - 1 - j + TX_QUEUE_SIZE_) % TX_QUEUE_SIZE_;
                    uint8_t to_pos = (tx_queue_tail_ - j) % TX_QUEUE_SIZE_;
                    tx_queue_[to_pos] = tx_queue_[from_pos];
                }
                break;
            }
        }
    }
    
    tx_queue_[insert_pos] = item;
    tx_queue_tail_ = (tx_queue_tail_ + 1) % TX_QUEUE_SIZE_;
    tx_queue_count_++;
}

void CanMessenger::insertRxWithPriority(const RxQueueItem& item) {
    uint8_t insert_pos = rx_queue_tail_;
    
    if (rx_queue_count_ > 0) {
        for (uint8_t i = 0; i < rx_queue_count_; i++) {
            uint8_t check_pos = (rx_queue_head_ + i) % RX_QUEUE_SIZE_;
            if (rx_queue_[check_pos].priority > item.priority) {
                insert_pos = check_pos;
                
                uint8_t shift_count = rx_queue_count_ - i;
                for (uint8_t j = 0; j < shift_count; j++) {
                    uint8_t from_pos = (rx_queue_tail_ - 1 - j + RX_QUEUE_SIZE_) % RX_QUEUE_SIZE_;
                    uint8_t to_pos = (rx_queue_tail_ - j) % RX_QUEUE_SIZE_;
                    rx_queue_[to_pos] = rx_queue_[from_pos];
                }
                break;
            }
        }
    }
    
    rx_queue_[insert_pos] = item;
    rx_queue_tail_ = (rx_queue_tail_ + 1) % RX_QUEUE_SIZE_;
    rx_queue_count_++;
}

bool CanMessenger::dequeueTxMessage(TxQueueItem& item) {
    if (isTxQueueEmpty()) {
        return false;
    }
    
    uint8_t best_index = tx_queue_head_;
    TxPriority best_priority = tx_queue_[tx_queue_head_].priority;
    
    for (uint8_t i = 0; i < tx_queue_count_; i++) {
        uint8_t check_index = (tx_queue_head_ + i) % TX_QUEUE_SIZE_;
        if (tx_queue_[check_index].priority < best_priority) {
            best_priority = tx_queue_[check_index].priority;
            best_index = check_index;
        }
    }
    
    item = tx_queue_[best_index];
    
    if (best_index != tx_queue_head_) {
        while (best_index != tx_queue_head_) {
            uint8_t prev_index = (best_index - 1 + TX_QUEUE_SIZE_) % TX_QUEUE_SIZE_;
            tx_queue_[best_index] = tx_queue_[prev_index];
            best_index = prev_index;
        }
    }
    
    tx_queue_head_ = (tx_queue_head_ + 1) % TX_QUEUE_SIZE_;
    tx_queue_count_--;
    
    return true;
}

bool CanMessenger::dequeueRxMessage(RxQueueItem& item) {
    if (isRxQueueEmpty()) {
        return false;
    }
    
    uint8_t best_index = rx_queue_head_;
    RxPriority best_priority = rx_queue_[rx_queue_head_].priority;
    
    for (uint8_t i = 0; i < rx_queue_count_; i++) {
        uint8_t check_index = (rx_queue_head_ + i) % RX_QUEUE_SIZE_;
        if (rx_queue_[check_index].priority < best_priority) {
            best_priority = rx_queue_[check_index].priority;
            best_index = check_index;
        }
    }
    
    item = rx_queue_[best_index];
    
    if (best_index != rx_queue_head_) {
        while (best_index != rx_queue_head_) {
            uint8_t prev_index = (best_index - 1 + RX_QUEUE_SIZE_) % RX_QUEUE_SIZE_;
            rx_queue_[best_index] = rx_queue_[prev_index];
            best_index = prev_index;
        }
    }
    
    rx_queue_head_ = (rx_queue_head_ + 1) % RX_QUEUE_SIZE_;
    rx_queue_count_--;
    
    return true;
}

InterfaceStatus CanMessenger::sendQueuedMessage(const TxQueueItem& item) {
	if (comm_can_ == nullptr) {
		return InterfaceStatus::INTERFACE_ERROR;
	}

    const bluelink::J1939CanIdStruct can_id(
        source_component_id_,
		item.destination_id,
		item.payload_type_id);

	comm_can_->setTxCanIdType(CommCan::CanIdType::CAN_ID_TYPE_EXT);
	comm_can_->setTxCanId(CONVERT_CAN_ID_TO_UINT32(can_id));
	comm_can_->setTxDlc(item.length);
	comm_can_->setTxRtrType(CommCan::CanRtrType::CAN_RTR_DATA_TYPE);

	InterfaceStatus result = comm_can_->startTransmitInterrupt(item.data, item.length);

	return result;
}

CanMessenger::RxPriority CanMessenger::determineRxPriority(uint8_t payload_type_id) {
    switch (static_cast<bluelink::PayloadTypeIds>(payload_type_id)) {
            case bluelink::PayloadTypeIds::PROGRAMMING_COMMAND:
        return RxPriority::PROGRAMMING_COMMAND;
    case bluelink::PayloadTypeIds::RESET_COMMAND:
        return RxPriority::RESET_COMMAND;
    case bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY:
        return RxPriority::METADATA_REQUEST;
    case bluelink::PayloadTypeIds::BLUELINK_VERSION_TELEMETRY:
        return RxPriority::METADATA_REQUEST;
    case bluelink::PayloadTypeIds::DRIVER_STATE_COMMAND:
        return RxPriority::DRIVER_STATE_COMMAND;
    case bluelink::PayloadTypeIds::REVERSER_COMMAND:
        return RxPriority::REVERSER_COMMAND;
    case bluelink::PayloadTypeIds::TRANSM_OUT_SPD_TELEMETRY:
        return RxPriority::TRANSM_OUT_SPD_TELEMETRY;
    default:
        return RxPriority::DEFAULT_RX_PRIORITY;
    }
}

CanMessenger::TxPriority CanMessenger::determineTxPriority(uint8_t payload_type_id) {
    switch (static_cast<bluelink::PayloadTypeIds>(payload_type_id)) {
    case bluelink::PayloadTypeIds::PROGRAMMING_COMMAND:
        return TxPriority::PROGRAMMING_COMMAND;
    case bluelink::PayloadTypeIds::CONTROLLER_META_DATA_TELEMETRY:
        return TxPriority::METADATA;
    case bluelink::PayloadTypeIds::BLUELINK_VERSION_TELEMETRY:
        return TxPriority::METADATA;
    case bluelink::PayloadTypeIds::REVERSER_TELEMETRY:
        return TxPriority::MAIN_TELEMETRY;
    case bluelink::PayloadTypeIds::REVERSER_ANALOG_CHANNEL_TELEMETRY:
        return TxPriority::ANALOG_TELEMETRY;
    default:
        return TxPriority::DEFAULT_TX_PRIORITY;
    }
}

void CanMessenger::resetStatistics() {
    enterCriticalSection();
    
    stats_.tx_messages_enqueued = 0;
    stats_.tx_messages_sent = 0;
    stats_.tx_queue_full_drops = 0;
    stats_.tx_busy_retries = 0;
    stats_.tx_interrupt_sends = 0;
    stats_.tx_tick_sends = 0;
    stats_.rx_messages_enqueued = 0;
    stats_.rx_messages_processed = 0;
    stats_.rx_queue_full_drops = 0;
    stats_.rx_tick_processes = 0;
    
    exitCriticalSection();
}

void CanMessenger::enterCriticalSection() {
    critical_section_state_ = __get_PRIMASK();
    __disable_irq();
}

void CanMessenger::exitCriticalSection() {
    __set_PRIMASK(critical_section_state_);
}
