#ifndef MICRO_SEQUENCE_EXECUTOR_HPP_
#define MICRO_SEQUENCE_EXECUTOR_HPP_

#include <cstdint>
#include "micro_var_store.hpp"
#include "sequences_configs.hpp"

class RawCanInterface;
class GpcController;
class SafetyFeatures;

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
  void setVarStore(MicroVarStore* var_store);
  void setGpcController(GpcController* gpc_controller);
  void setSafetyFeatures(SafetyFeatures* safety_features);

  bool isRunning() const;
  State getState() const;

  bool start(const volatile MicroSequence& sequence, bool loop_on_complete = false);
  void stop();
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
  bool executeVarSet(const bluelink::MicroOpsPayload::MicroVarSet& op);
  bool executeMoveToErrorState(const bluelink::MicroOpsPayload::MicroMoveToErrorState& op);
  bool executeMoveToEmergencyState(const bluelink::MicroOpsPayload::MicroMoveToEmergencyState& op);
  bool executeTriggerSafety(const bluelink::MicroOpsPayload::MicroTriggerSafety& op);
  bool evaluateCondition(const bluelink::MicroOpsPayload::MicroIfCondition& op) const;

  RawCanInterface* raw_can_ = nullptr;
  MicroVarStore* var_store_ = nullptr;
  GpcController* gpc_controller_ = nullptr;
  SafetyFeatures* safety_features_ = nullptr;
  const volatile MicroSequence* active_sequence_ = nullptr;
  uint8_t step_index_ = 0;
  bool loop_on_complete_ = false;
  State state_ = State::IDLE;
  uint32_t delay_start_ms_ = 0;
  uint32_t delay_duration_ms_ = 0;
};

#endif  // MICRO_SEQUENCE_EXECUTOR_HPP_
