#ifndef MICRO_SEQUENCE_EXECUTOR_HPP_
#define MICRO_SEQUENCE_EXECUTOR_HPP_

#include <cstdint>
#include "sequences_configs.hpp"

class RawCanInterface;

class MicroSequenceExecutor {
 public:
  enum class State : uint8_t {
    IDLE,
    RUNNING,
    WAITING_DELAY,
    COMPLETED,
    ERROR,
  };

  MicroSequenceExecutor();

  void setRawCanInterface(RawCanInterface* raw_can);

  bool isRunning() const;
  State getState() const;

  bool start(const volatile MicroSequence& sequence);
  bool executeImmediateOp(const bluelink::MicroOpsPayload::MicroOpStep& step);
  void tick();

 private:
  bool isDelayDue() const;
  void startDelay(uint32_t delay_ms);

  bool executeStep(const bluelink::MicroOpsPayload::MicroOpStep& step);
  bool executeDigitalGpioWrite(const bluelink::MicroOpsPayload::MicroDigitalGpioWrite& op);
  bool executeDigitalGpioRead(const bluelink::MicroOpsPayload::MicroDigitalGpioRead& op);
  bool executeAdcRead(const bluelink::MicroOpsPayload::MicroAdcRead& op);
  bool executeDacWrite(const bluelink::MicroOpsPayload::MicroDacWrite& op);
  bool executePwmSet(const bluelink::MicroOpsPayload::MicroPwmSet& op);
  bool executeCanTransmit(const bluelink::MicroOpsPayload::MicroCanTransmit& op);
  bool executeUartTransmit(const bluelink::MicroOpsPayload::MicroUartTransmit& op);
  bool executeSpiTransfer(const bluelink::MicroOpsPayload::MicroSpiTransfer& op);
  bool executeI2cWrite(const bluelink::MicroOpsPayload::MicroI2cWrite& op);

  RawCanInterface* raw_can_ = nullptr;
  const volatile MicroSequence* active_sequence_ = nullptr;
  uint8_t step_index_ = 0;
  State state_ = State::IDLE;
  uint32_t vars_[MICRO_VAR_SLOT_COUNT] = {};
  uint32_t delay_start_ms_ = 0;
  uint32_t delay_duration_ms_ = 0;
};

#endif  // MICRO_SEQUENCE_EXECUTOR_HPP_
