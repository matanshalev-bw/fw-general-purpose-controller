/*
 * adc_buffer_defines.hpp
 *
 * Created on: Jan 16, 2025
 * Author: ariel
 */

#ifndef INTERFACE_ADC_BUFFER_DEFINES_HPP_
#define INTERFACE_ADC_BUFFER_DEFINES_HPP_

#include <stdint.h>
#include "stm32g4xx_hal.h"

#ifdef HAL_ADC_MODULE_ENABLED
enum class AdcInstance : uint8_t {
    ADC_1 = 0,
    ADC_2 = 1,
    ADC_3 = 2,
    ADC_COUNT = 3
};

enum class AdcChannel : uint32_t {
    CHANNEL_1 = ADC_CHANNEL_1,
    CHANNEL_2 = ADC_CHANNEL_2,
    CHANNEL_3 = ADC_CHANNEL_3,
    CHANNEL_4 = ADC_CHANNEL_4,
    CHANNEL_5 = ADC_CHANNEL_5,
    CHANNEL_11 = ADC_CHANNEL_11,
    CHANNEL_12 = ADC_CHANNEL_12,
    CHANNEL_15 = ADC_CHANNEL_15
};

struct AdcChannelConfig {
    AdcChannel channel;
    uint32_t rank;
    uint32_t sampling_time;
    float voltage_divider_ratio;
    
    AdcChannelConfig() : channel(AdcChannel::CHANNEL_1), rank(1), sampling_time(ADC_SAMPLETIME_640CYCLES_5), voltage_divider_ratio(1.0f) {}
    AdcChannelConfig(AdcChannel ch, uint32_t r, uint32_t st, float vdr) 
        : channel(ch), rank(r), sampling_time(st), voltage_divider_ratio(vdr) {}
};

struct AdcBufferData {
    uint16_t raw_values[16];  // Support up to 16 channels per ADC
    uint8_t channel_count;
    bool data_ready;
    uint32_t timestamp;
    
    AdcBufferData() : channel_count(0), data_ready(false), timestamp(0) {
        for (int i = 0; i < 16; i++) {
            raw_values[i] = 0;
        }
    }
};

struct AdcDmaConfig {
    AdcInstance adc_instance;
    uint8_t channel_count;
    AdcChannelConfig channels[16];
    uint16_t* dma_buffer;
    uint32_t buffer_size;
    DMA_HandleTypeDef* dma_handle;
    ADC_HandleTypeDef* adc_handle;
    
    AdcDmaConfig() : adc_instance(AdcInstance::ADC_1), channel_count(0), dma_buffer(nullptr), 
                     buffer_size(0), dma_handle(nullptr), adc_handle(nullptr) {}
};
#endif // HAL_ADC_MODULE_ENABLED

#endif /* INTERFACE_ADC_BUFFER_DEFINES_HPP_ */
