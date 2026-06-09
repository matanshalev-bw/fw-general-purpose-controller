#ifndef GPC_CONTROLLER_HPP_
#define GPC_CONTROLLER_HPP_

#include "PayloadFieldDefinitions.hpp"
#include "bluelink_messages.hpp"
#include "micro_sequence_executor.hpp"
#include "sequences_configs.hpp"

class RawCanInterface;

class GpcController {
 public:
  GpcController();

  void setRawCanInterface(RawCanInterface* raw_can);

  bluelink::ControllerState getState() const { return state_; }

  bool sendSetStateRequest(bluelink::ControllerState req_state);
  bool handleControllerStateCommand(const bluelink::CommandsPayload::ControllerStateCommand& cmd);

  void tick();

 private:
  static const volatile MicroSequence* getStateSequence(const volatile SequencesConfig& sequences,
                                                        bluelink::ControllerState state);
  static bool isStateTickLoop(bluelink::ControllerState state);
  static bluelink::ControllerState nextStateAfterSequence(bluelink::ControllerState state);

  void enterState(bluelink::ControllerState new_state);
  void onStateChanged();
  void tickMainSequence();
  void tickStateSequence();
  void onOneShotSequenceComplete();

  bluelink::ControllerState state_ = bluelink::ControllerState::CONTROLLER_STATE_MANUAL;
  bool state_sequence_started_ = false;
  MicroSequenceExecutor main_tick_executor_;
  MicroSequenceExecutor state_sequence_executor_;
};

#endif  // GPC_CONTROLLER_HPP_
