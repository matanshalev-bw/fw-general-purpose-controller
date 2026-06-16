#include "micro_sequence_executor.hpp"

#include <cstring>

#include "gpio_interface.hpp"
#include "raw_can_interface.hpp"
#include "system_interface.hpp"

#ifdef HAL_ADC_MODULE_ENABLED
#include "adc_manager.hpp"
#endif

extern UART_HandleTypeDef huart2;
extern SPI_HandleTypeDef hspi1;
extern I2C_HandleTypeDef hi2c1;

namespace {

GpioPortType toGpioPort(uint8_t port) { return static_cast<GpioPortType>(port); }

GpioPinNumber toGpioPin(uint8_t pin) { return static_cast<GpioPinNumber>(GPIO_PIN_0 << pin); }

}  // namespace

MicroSequenceExecutor::MicroSequenceExecutor() = default;

void MicroSequenceExecutor::setRawCanInterface(RawCanInterface* raw_can) { raw_can_ = raw_can; }

void MicroSequenceExecutor::setVarStore(MicroVarStore* var_store) { var_store_ = var_store; }

bool MicroSequenceExecutor::isDelayDue() const {
  if (delay_duration_ms_ == 0) {
    return true;
  }
  return (HAL_GetTick() - delay_start_ms_) >= delay_duration_ms_;
}

void MicroSequenceExecutor::startDelay(uint32_t delay_ms) {
  delay_start_ms_ = HAL_GetTick();
  delay_duration_ms_ = delay_ms;
  state_ = State::WAITING_DELAY;
}

bool MicroSequenceExecutor::isRunning() const {
  return state_ == State::RUNNING || state_ == State::WAITING_DELAY;
}

MicroSequenceExecutor::State MicroSequenceExecutor::getState() const { return state_; }

bool MicroSequenceExecutor::executeImmediateOp(const bluelink::MicroOpsPayload::MicroOpStep& step) {
  if (isRunning()) {
    return false;
  }

  if (step.op_type == bluelink::MicroOpsPayload::MicroOpType::DELAY_MS) {
    const auto* delay_op = reinterpret_cast<const bluelink::MicroOpsPayload::MicroDelayMs*>(step.params);
    SystemInterface::delay(delay_op->delay_ms);
    return true;
  }

  return executeStep(step);
}

bool MicroSequenceExecutor::start(const volatile MicroSequence& sequence, bool loop_on_complete) {
  if (isRunning()) {
    return false;
  }

  if (sequence.step_count == 0 || sequence.step_count > MICRO_SEQUENCE_MAX_STEPS) {
    return false;
  }

  active_sequence_ = &sequence;
  step_index_ = 0;
  loop_on_complete_ = loop_on_complete;
  state_ = State::RUNNING;
  delay_start_ms_ = 0;
  delay_duration_ms_ = 0;
  return true;
}

void MicroSequenceExecutor::stop() {
  active_sequence_ = nullptr;
  step_index_ = 0;
  loop_on_complete_ = false;
  state_ = State::IDLE;
  delay_start_ms_ = 0;
  delay_duration_ms_ = 0;
}

void MicroSequenceExecutor::tick() {
  if (state_ == State::IDLE || state_ == State::COMPLETED || state_ == State::ERROR) {
    return;
  }

  if (active_sequence_ == nullptr) {
    state_ = State::ERROR;
    return;
  }

  if (state_ == State::WAITING_DELAY) {
    if (not isDelayDue()) {
      return;
    }
    delay_duration_ms_ = 0;
    step_index_++;
    state_ = State::RUNNING;
  }

  while (step_index_ < active_sequence_->step_count) {
    bluelink::MicroOpsPayload::MicroOpStep step{};
    memcpy(&step, const_cast<const void*>(static_cast<const volatile void*>(&active_sequence_->steps[step_index_])),
           sizeof(step));

    if (step.op_type == bluelink::MicroOpsPayload::MicroOpType::DELAY_MS) {
      const auto* delay_op = reinterpret_cast<const bluelink::MicroOpsPayload::MicroDelayMs*>(step.params);
      startDelay(delay_op->delay_ms);
      return;
    }

    if (not executeStep(step)) {
      state_ = State::ERROR;
      active_sequence_ = nullptr;
      return;
    }

    step_index_++;
  }

  if (loop_on_complete_) {
    step_index_ = 0;
    state_ = State::RUNNING;
    return;
  }

  state_ = State::IDLE;
  active_sequence_ = nullptr;
  step_index_ = 0;
}

bool MicroSequenceExecutor::executeStep(const bluelink::MicroOpsPayload::MicroOpStep& step) {
  switch (step.op_type) {
    case bluelink::MicroOpsPayload::MicroOpType::NOP:
      return true;
    case bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE:
      return executeDigitalGpioWrite(*reinterpret_cast<const bluelink::MicroOpsPayload::MicroDigitalGpioWrite*>(step.params));
    case bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_READ:
      return executeDigitalGpioRead(*reinterpret_cast<const bluelink::MicroOpsPayload::MicroDigitalGpioRead*>(step.params));
    case bluelink::MicroOpsPayload::MicroOpType::ADC_READ:
      return executeAdcRead(*reinterpret_cast<const bluelink::MicroOpsPayload::MicroAdcRead*>(step.params));
    case bluelink::MicroOpsPayload::MicroOpType::DAC_WRITE:
      return executeDacWrite(*reinterpret_cast<const bluelink::MicroOpsPayload::MicroDacWrite*>(step.params));
    case bluelink::MicroOpsPayload::MicroOpType::PWM_SET:
      return executePwmSet(*reinterpret_cast<const bluelink::MicroOpsPayload::MicroPwmSet*>(step.params));
    case bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT:
      return executeCanTransmit(*reinterpret_cast<const bluelink::MicroOpsPayload::MicroCanTransmit*>(step.params));
    case bluelink::MicroOpsPayload::MicroOpType::UART_TRANSMIT:
      return executeUartTransmit(*reinterpret_cast<const bluelink::MicroOpsPayload::MicroUartTransmit*>(step.params));
    case bluelink::MicroOpsPayload::MicroOpType::SPI_TRANSFER:
      return executeSpiTransfer(*reinterpret_cast<const bluelink::MicroOpsPayload::MicroSpiTransfer*>(step.params));
    case bluelink::MicroOpsPayload::MicroOpType::I2C_WRITE:
      return executeI2cWrite(*reinterpret_cast<const bluelink::MicroOpsPayload::MicroI2cWrite*>(step.params));
    default:
      return false;
  }
}

bool MicroSequenceExecutor::executeDigitalGpioWrite(const bluelink::MicroOpsPayload::MicroDigitalGpioWrite& op) {
  const GpioPin pin = GpioInterface::createDigitalGpio(::toGpioPort(op.port), ::toGpioPin(op.pin));
  const GpioPinState state = op.value != 0 ? GpioPinState::PIN_SET : GpioPinState::PIN_RESET;
  return GpioInterface::digitalWrite(pin, state) == InterfaceStatus::INTERFACE_OK;
}

bool MicroSequenceExecutor::executeDigitalGpioRead(const bluelink::MicroOpsPayload::MicroDigitalGpioRead& op) {
  if (op.var_index >= MICRO_VAR_SLOT_COUNT) {
    return false;
  }

  if (var_store_ == nullptr) {
    return false;
  }

  const GpioPin pin = GpioInterface::createDigitalGpio(::toGpioPort(op.port), ::toGpioPin(op.pin));
  GpioPinState state = GpioPinState::PIN_RESET;
  if (GpioInterface::digitalRead(pin, state) != InterfaceStatus::INTERFACE_OK) {
    return false;
  }

  var_store_->set(op.var_index, state == GpioPinState::PIN_SET ? 1U : 0U);
  return true;
}

bool MicroSequenceExecutor::executeAdcRead(const bluelink::MicroOpsPayload::MicroAdcRead& op) {
  if (op.var_index >= MICRO_VAR_SLOT_COUNT || var_store_ == nullptr) {
    return false;
  }

#ifdef HAL_ADC_MODULE_ENABLED
  if (op.adc_instance == 0 || op.adc_instance > static_cast<uint8_t>(AdcInstance::ADC_COUNT)) {
    return false;
  }

  const AdcInstance adc_instance = static_cast<AdcInstance>(op.adc_instance - 1U);
  uint16_t raw_value = 0;
  if (AdcManager::getInstance()->getChannelValue(adc_instance, op.channel, raw_value) != InterfaceStatus::INTERFACE_OK) {
    return false;
  }

  if (op.store_raw != 0) {
    var_store_->set(op.var_index, raw_value);
  } else {
    float voltage = 0.0f;
    if (GpioInterface::analogReadDma(adc_instance, op.channel, voltage) != InterfaceStatus::INTERFACE_OK) {
      var_store_->set(op.var_index, raw_value);
    } else {
      var_store_->set(op.var_index, static_cast<uint32_t>(voltage * 1000.0f));
    }
  }
  return true;
#else
  (void)op;
  return false;
#endif
}

bool MicroSequenceExecutor::executeDacWrite(const bluelink::MicroOpsPayload::MicroDacWrite& op) {
#ifdef HAL_DAC_MODULE_ENABLED
  (void)op;
  return false;
#else
  (void)op;
  return false;
#endif
}

bool MicroSequenceExecutor::executePwmSet(const bluelink::MicroOpsPayload::MicroPwmSet& op) {
  (void)op;
  return false;
}

bool MicroSequenceExecutor::executeCanTransmit(const bluelink::MicroOpsPayload::MicroCanTransmit& op) {
  if (raw_can_ == nullptr || op.can_bus != 1) {
    return false;
  }

  const uint8_t dlc = op.dlc > 8 ? 8 : op.dlc;
  return raw_can_->transmitStandard(op.id, op.data, dlc) == InterfaceStatus::INTERFACE_OK;
}

bool MicroSequenceExecutor::executeUartTransmit(const bluelink::MicroOpsPayload::MicroUartTransmit& op) {
  if (op.uart_instance != 2 || op.length == 0 || op.length > 8) {
    return false;
  }

  return HAL_UART_Transmit(&huart2, const_cast<uint8_t*>(op.data), op.length, 100) == HAL_OK;
}

bool MicroSequenceExecutor::executeSpiTransfer(const bluelink::MicroOpsPayload::MicroSpiTransfer& op) {
  if (op.spi_instance != 2 || op.tx_len == 0 || op.tx_len > 8) {
    return false;
  }

  uint8_t rx_data[8] = {};
  return HAL_SPI_TransmitReceive(&hspi1, const_cast<uint8_t*>(op.tx_data), rx_data, op.tx_len, 100) == HAL_OK;
}

bool MicroSequenceExecutor::executeI2cWrite(const bluelink::MicroOpsPayload::MicroI2cWrite& op) {
  if (op.i2c_instance != 1 || op.length == 0 || op.length > 8) {
    return false;
  }

  return HAL_I2C_Master_Transmit(&hi2c1, static_cast<uint16_t>(op.device_addr << 1), const_cast<uint8_t*>(op.data),
                                 op.length, 100) == HAL_OK;
}
