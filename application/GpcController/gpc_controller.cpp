#include "gpc_controller.hpp"

#include "non_volatile_memory_interface.hpp"
#include "raw_can_interface.hpp"

GpcController::GpcController() = default;

void GpcController::setRawCanInterface(RawCanInterface* raw_can) {
  main_tick_executor_.setRawCanInterface(raw_can);
  state_sequence_executor_.setRawCanInterface(raw_can);
}

void GpcController::setVarStore(MicroVarStore* var_store) {
  main_tick_executor_.setVarStore(var_store);
  state_sequence_executor_.setVarStore(var_store);
}

bool GpcController::sendSetStateRequest(bluelink::ControllerState req_state) {
  if (state_ == req_state ||
      (state_ == bluelink::ControllerState::CONTROLLER_STATE_OPERATIONAL &&
       req_state == bluelink::ControllerState::CONTROLLER_STATE_POWER_UP_BIT)) {
    return true;
  }

  if (req_state == bluelink::ControllerState::CONTROLLER_STATE_INIT &&
      state_ == bluelink::ControllerState::CONTROLLER_STATE_ENGAGED) {
    return true;
  }

  if (req_state == bluelink::ControllerState::CONTROLLER_STATE_DISENGAGEMENT &&
      state_ == bluelink::ControllerState::CONTROLLER_STATE_MANUAL) {
    return true;
  }

  if (req_state == bluelink::ControllerState::CONTROLLER_STATE_OPERATIONAL ||
      req_state == bluelink::ControllerState::CONTROLLER_STATE_ERROR ||
      req_state == bluelink::ControllerState::CONTROLLER_STATE_EMERGENCY ||
      req_state == bluelink::ControllerState::CONTROLLER_STATE_TECHNICIAN) {
    return false;
  }

  enterState(req_state);
  return true;
}

bool GpcController::handleControllerStateCommand(
    const bluelink::CommandsPayload::ControllerStateCommand& cmd) {
  return sendSetStateRequest(static_cast<bluelink::ControllerState>(cmd.controller_state));
}

void GpcController::enterState(bluelink::ControllerState new_state) {
  state_ = new_state;
  onStateChanged();
}

void GpcController::onStateChanged() {
  state_sequence_executor_.stop();
  state_sequence_started_ = false;
}

const volatile MicroSequence* GpcController::getStateSequence(const volatile SequencesConfig& sequences,
                                                            bluelink::ControllerState state) {
  switch (state) {
    case bluelink::ControllerState::CONTROLLER_STATE_INIT:
      return &sequences.init_state_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_MANUAL:
      return &sequences.manual_state_tick_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_DISENGAGEMENT:
      return &sequences.disengagement_state_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_ENGAGED:
      return &sequences.engaged_state_tick_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_POWER_UP_BIT:
      return &sequences.power_up_bit_state_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_OPERATIONAL:
      return &sequences.operational_state_tick_sequence;
    default:
      return nullptr;
  }
}

bool GpcController::isStateTickLoop(bluelink::ControllerState state) {
  switch (state) {
    case bluelink::ControllerState::CONTROLLER_STATE_MANUAL:
    case bluelink::ControllerState::CONTROLLER_STATE_ENGAGED:
    case bluelink::ControllerState::CONTROLLER_STATE_OPERATIONAL:
      return true;
    default:
      return false;
  }
}

bluelink::ControllerState GpcController::nextStateAfterSequence(bluelink::ControllerState state) {
  switch (state) {
    case bluelink::ControllerState::CONTROLLER_STATE_INIT:
      return bluelink::ControllerState::CONTROLLER_STATE_ENGAGED;
    case bluelink::ControllerState::CONTROLLER_STATE_DISENGAGEMENT:
      return bluelink::ControllerState::CONTROLLER_STATE_MANUAL;
    case bluelink::ControllerState::CONTROLLER_STATE_POWER_UP_BIT:
      return bluelink::ControllerState::CONTROLLER_STATE_OPERATIONAL;
    default:
      return state;
  }
}

void GpcController::onOneShotSequenceComplete() {
  const bluelink::ControllerState next_state = nextStateAfterSequence(state_);
  if (next_state != state_) {
    enterState(next_state);
  }
}

void GpcController::tickMainSequence() {
  const volatile SequencesConfig& sequences = NonVolatileMemoryInterface::CONFIG_MEMORY_.sequences_config;
  main_tick_executor_.tick();
  if (not main_tick_executor_.isRunning() && sequences.main_tick_sequence.step_count > 0) {
    main_tick_executor_.start(sequences.main_tick_sequence, true);
  }
}

void GpcController::tickStateSequence() {
  const volatile SequencesConfig& sequences = NonVolatileMemoryInterface::CONFIG_MEMORY_.sequences_config;
  const volatile MicroSequence* state_sequence = getStateSequence(sequences, state_);
  if (state_sequence == nullptr) {
    return;
  }

  const bool loop_on_complete = isStateTickLoop(state_);
  if (state_sequence->step_count == 0) {
    if (not loop_on_complete) {
      onOneShotSequenceComplete();
    }
    return;
  }

  state_sequence_executor_.tick();

  if (state_sequence_executor_.isRunning()) {
    return;
  }

  if (state_sequence_started_ && not loop_on_complete) {
    state_sequence_started_ = false;
    onOneShotSequenceComplete();
    return;
  }

  if (not state_sequence_started_) {
    state_sequence_started_ = state_sequence_executor_.start(*state_sequence, loop_on_complete);
  }
}

void GpcController::tick() {
  tickMainSequence();
  tickStateSequence();
}
