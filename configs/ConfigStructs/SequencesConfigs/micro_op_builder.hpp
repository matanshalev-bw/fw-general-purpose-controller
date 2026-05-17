#ifndef FW_MICRO_OP_BUILDER_HPP_
#define FW_MICRO_OP_BUILDER_HPP_

#include <cstring>
#include "MicroOpsPayloadClasses.hpp"

namespace micro_op_builder {

inline bluelink::MicroOpsPayload::MicroOpStep makeStep(bluelink::MicroOpsPayload::MicroOpType type,
                                                       const void* params, uint8_t params_size) {
  bluelink::MicroOpsPayload::MicroOpStep step{};
  step.op_type = type;
  if (params != nullptr && params_size > 0) {
    const uint8_t copy_size = params_size > bluelink::MicroOpsPayload::MICRO_OP_PARAMS_SIZE
                                ? bluelink::MicroOpsPayload::MICRO_OP_PARAMS_SIZE
                                : params_size;
    memcpy(step.params, params, copy_size);
  }
  return step;
}

inline bluelink::MicroOpsPayload::MicroOpStep digitalGpioWrite(uint8_t port, uint8_t pin, uint8_t value) {
  const bluelink::MicroOpsPayload::MicroDigitalGpioWrite params{port, pin, value};
  return makeStep(bluelink::MicroOpsPayload::MicroOpType::DIGITAL_GPIO_WRITE, &params, sizeof(params));
}

inline bluelink::MicroOpsPayload::MicroOpStep adcRead(uint8_t adc_instance, uint8_t channel, uint8_t var_index,
                                                      uint8_t store_raw = 1) {
  const bluelink::MicroOpsPayload::MicroAdcRead params{adc_instance, channel, var_index, store_raw};
  return makeStep(bluelink::MicroOpsPayload::MicroOpType::ADC_READ, &params, sizeof(params));
}

inline bluelink::MicroOpsPayload::MicroOpStep dacWriteFromVar(uint8_t dac_instance, uint8_t var_index) {
  const bluelink::MicroOpsPayload::MicroDacWrite params{dac_instance, 1, var_index, 0};
  return makeStep(bluelink::MicroOpsPayload::MicroOpType::DAC_WRITE, &params, sizeof(params));
}

inline bluelink::MicroOpsPayload::MicroOpStep delayMs(uint32_t delay_ms) {
  const bluelink::MicroOpsPayload::MicroDelayMs params{delay_ms};
  return makeStep(bluelink::MicroOpsPayload::MicroOpType::DELAY_MS, &params, sizeof(params));
}

inline bluelink::MicroOpsPayload::MicroOpStep canTransmit(uint8_t can_bus, uint32_t id, uint8_t dlc,
                                                          const uint8_t* data) {
  bluelink::MicroOpsPayload::MicroCanTransmit params{};
  params.can_bus = can_bus;
  params.dlc = dlc;
  params.id = id;
  if (data != nullptr) {
    memcpy(params.data, data, sizeof(params.data));
  }
  return makeStep(bluelink::MicroOpsPayload::MicroOpType::CAN_TRANSMIT, &params, sizeof(params));
}

}  // namespace micro_op_builder

#endif  // FW_MICRO_OP_BUILDER_HPP_
