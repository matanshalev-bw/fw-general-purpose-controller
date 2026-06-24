/*
 * can_messenger.hpp
 *
 * Created on: Jan 2, 2025
 * Author: ariel
 */

#ifndef CAN_MESSENGER_HPP_
#define CAN_MESSENGER_HPP_

#include <cstdint>
#include <cstring>
#include "comm_defines.hpp"
#include "interface_status.hpp"
#include "distributed_can_id.hpp"
#include "bluelink_messages.hpp"

class CommCan;

class CanMessenger {
 public:
  enum class TxPriority : uint8_t {
     PROGRAMMING_COMMAND = 0,
     METADATA = 1,
     MAIN_TELEMETRY = 2,
     ANALOG_TELEMETRY = 3,
 	 DEFAULT_TX_PRIORITY = 4
   };

   enum class RxPriority : uint8_t {
     PROGRAMMING_COMMAND = 0,
     RESET_COMMAND = 1,
     METADATA_REQUEST = 2,
     DRIVER_STATE_COMMAND = 3,
     REVERSER_COMMAND = 4,
     TRANSM_OUT_SPD_TELEMETRY = 5,
 	   DEFAULT_RX_PRIORITY = 6
   };

  static constexpr uint8_t MAX_RX_PROCESS_PER_TICK_ = 4;

 private:
  static constexpr uint8_t TX_QUEUE_SIZE_ = 32;
  static constexpr uint8_t RX_QUEUE_SIZE_ = 16;
  static constexpr uint8_t MAX_INTERRUPT_SENDS_ = 2;

 public:
  struct TxQueueItem {
    TxPriority priority;
    uint8_t payload_type_id;
    uint8_t destination_id;
    uint8_t data[8];
    uint8_t length;
    uint32_t timestamp;
    bool pending;

    TxQueueItem() : priority(TxPriority::ANALOG_TELEMETRY), payload_type_id(0),
                   destination_id(0), length(0), timestamp(0), pending(false) {
      memset(data, 0, sizeof(data));
    }
  };

  struct RxQueueItem {
    RxPriority priority;
    uint8_t payload_type_id;
    uint8_t source_id;
    uint8_t destination_id;
    uint8_t data[8];
    uint8_t length;
    uint32_t timestamp;
    uint32_t can_id;
    bool pending;

    RxQueueItem() : priority(RxPriority::METADATA_REQUEST), payload_type_id(0),
                   source_id(0), destination_id(0), length(0), timestamp(0), 
                   can_id(0), pending(false) {
      memset(data, 0, sizeof(data));
    }
  };

  struct Statistics {
    uint32_t tx_messages_enqueued;
    uint32_t tx_messages_sent;
    uint32_t tx_queue_full_drops;
    uint32_t tx_busy_retries;
    uint32_t tx_interrupt_sends;
    uint32_t tx_tick_sends;
    uint32_t rx_messages_enqueued;
    uint32_t rx_messages_processed;
    uint32_t rx_queue_full_drops;
    uint32_t rx_tick_processes;
  };

  CanMessenger(CommCan* comm_can, bluelink::ComponentId source_component_id);

  bool enqueueTxMessage(const CommCanTxHeader& tx_header, const uint8_t* data, uint8_t length);
  bool enqueueRxMessage(const CommCanRxHeader& rx_header, const uint8_t* data, uint8_t length);
  bool enqueueRxMessageFromInterrupt(const CommCanRxHeader& rx_header, const uint8_t* data, uint8_t length);  // Optimized for interrupt context
  bool getNextRxMessage(RxQueueItem& item);
  
  void processQueueFromTick();
  void processQueueFromInterrupt();
  bool processRxQueueFromTick();

  inline bool isTxQueueEmpty() const { return tx_queue_count_ == 0; }
  inline bool isTxQueueFull() const { return tx_queue_count_ >= TX_QUEUE_SIZE_; }
  inline uint8_t getTxQueueCount() const { return tx_queue_count_; }
  inline uint8_t getTxFreeSlots() const { return TX_QUEUE_SIZE_ - tx_queue_count_; }
  
  inline bool isRxQueueEmpty() const { return rx_queue_count_ == 0; }
  inline bool isRxQueueFull() const { return rx_queue_count_ >= RX_QUEUE_SIZE_; }
  inline uint8_t getRxQueueCount() const { return rx_queue_count_; }
  inline uint8_t getRxFreeSlots() const { return RX_QUEUE_SIZE_ - rx_queue_count_; }

  inline const Statistics& getStatistics() const { return stats_; }
  void resetStatistics();

  static RxPriority determineRxPriority(uint8_t payload_type_id);
  static TxPriority determineTxPriority(uint8_t payload_type_id);

 private:
  CommCan* comm_can_;
  bluelink::ComponentId source_component_id_;
  uint32_t critical_section_state_;

  static TxQueueItem tx_queue_[TX_QUEUE_SIZE_];
  static volatile uint8_t tx_queue_head_;
  static volatile uint8_t tx_queue_tail_;
  static volatile uint8_t tx_queue_count_;
  
  static RxQueueItem rx_queue_[RX_QUEUE_SIZE_];
  static volatile uint8_t rx_queue_head_;
  static volatile uint8_t rx_queue_tail_;
  static volatile uint8_t rx_queue_count_;
  
  static Statistics stats_;

  bool dequeueTxMessage(TxQueueItem& item);
  bool dequeueRxMessage(RxQueueItem& item);
  void insertTxWithPriority(const TxQueueItem& item);
  void insertRxWithPriority(const RxQueueItem& item);
  
  InterfaceStatus sendQueuedMessage(const TxQueueItem& item);
  
  void enterCriticalSection();
  void exitCriticalSection();
};

#endif /* CAN_MESSENGER_HPP_ */
