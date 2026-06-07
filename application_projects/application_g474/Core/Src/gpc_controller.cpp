#include "gpc_controller.hpp"

#include "non_volatile_memory_interface.hpp"
#include "raw_can_interface.hpp"

GpcController::GpcController() = default;

void GpcController::setRawCanInterface(RawCanInterface* raw_can) {
  main_tick_executor_.setRawCanInterface(raw_can);
  state_tick_executor_.setRawCanInterface(raw_can);
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

  if (state_ == bluelink::ControllerState::CONTROLLER_STATE_EMERGENCY ||
      req_state == bluelink::ControllerState::CONTROLLER_STATE_OPERATIONAL ||
      req_state == bluelink::ControllerState::CONTROLLER_STATE_ERROR ||
      req_state == bluelink::ControllerState::CONTROLLER_STATE_EMERGENCY) {
    return false;
  }

  state_ = req_state;
  onStateChanged();
  return true;
}

bool GpcController::handleControllerStateCommand(
    const bluelink::CommandsPayload::ControllerStateCommand& cmd) {
  return sendSetStateRequest(static_cast<bluelink::ControllerState>(cmd.controller_state));
}

void GpcController::onStateChanged() {
  state_tick_executor_.stop();
}

const volatile MicroSequence* GpcController::getStateTickSequence(const volatile SequencesConfig& sequences,
                                                                 bluelink::ControllerState state) {
  switch (state) {
    case bluelink::ControllerState::CONTROLLER_STATE_INIT:
      return &sequences.init_state_tick_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_MANUAL:
      return &sequences.manual_state_tick_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_DISENGAGEMENT:
      return &sequences.disengagement_state_tick_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_ENGAGED:
      return &sequences.engaged_state_tick_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_POWER_UP_BIT:
      return &sequences.power_up_bit_state_tick_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_OPERATIONAL:
      return &sequences.operational_state_tick_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_ERROR:
      return &sequences.error_state_tick_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_EMERGENCY:
      return &sequences.emergency_state_tick_sequence;
    case bluelink::ControllerState::CONTROLLER_STATE_TECHNICIAN:
      return &sequences.technician_state_tick_sequence;
    default:
      return nullptr;
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
  const volatile MicroSequence* state_sequence = getStateTickSequence(sequences, state_);
  if (state_sequence == nullptr) {
    return;
  }

  state_tick_executor_.tick();
  if (not state_tick_executor_.isRunning() && state_sequence->step_count > 0) {
    state_tick_executor_.start(*state_sequence, true);
  }
}

void GpcController::tick() {
  switch (state_) {
    case bluelink::ControllerState::CONTROLLER_STATE_MANUAL:
      break;
    default:
      break;
  }

  tickMainSequence();
  tickStateSequence();
}
