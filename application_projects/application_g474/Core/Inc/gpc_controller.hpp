#ifndef GPC_CONTROLLER_HPP_
#define GPC_CONTROLLER_HPP_

#include "PayloadFieldDefinitions.hpp"
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
  static const volatile MicroSequence* getStateTickSequence(const volatile SequencesConfig& sequences,
                                                            bluelink::ControllerState state);
  void onStateChanged();
  void tickMainSequence();
  void tickStateSequence();

  bluelink::ControllerState state_ = bluelink::ControllerState::CONTROLLER_STATE_MANUAL;
  MicroSequenceExecutor main_tick_executor_;
  MicroSequenceExecutor state_tick_executor_;
};

#endif  // GPC_CONTROLLER_HPP_
