/*
 * adc_manager.hpp
 *
 * Created on: Jan 16, 2025
 * Author: ariel
 */

#ifndef INTERFACE_ADC_MANAGER_HPP_
#define INTERFACE_ADC_MANAGER_HPP_


#include <stdint.h>
#include "adc_buffer_defines.hpp"
#include "comm_defines.hpp"
#include "interface_status.hpp"
#include "stm32g4xx_hal.h"
#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32g4xx_ll_adc.h"

class AdcManager {
private:
    static constexpr uint8_t MAX_ADC_CHANNELS_ = 16;
    static constexpr float VOLTAGE_DIVIDER_RATIO_ = (11.0f / (5.1f + 11.0f));
    static constexpr uint8_t ADC1_BUFFER_SIZE_ = 7;
    static constexpr uint8_t ADC2_BUFFER_SIZE_ = 1;
    static constexpr uint8_t ADC3_BUFFER_SIZE_ = 1;

    static AdcManager* instance_;
    
    AdcDmaConfig adc_configs_[static_cast<uint8_t>(AdcInstance::ADC_COUNT)];
    AdcBufferData adc_data_[static_cast<uint8_t>(AdcInstance::ADC_COUNT)];
    
    uint16_t dma_buffer_adc1_[ADC1_BUFFER_SIZE_];
    uint16_t dma_buffer_adc2_[ADC2_BUFFER_SIZE_];
    uint16_t dma_buffer_adc3_[ADC3_BUFFER_SIZE_];
    
    static uint32_t adc_interrupt_flags_;
    bool is_initialized_ = false;
    
    AdcManager();
    AdcManager(const AdcManager&) = delete;
    AdcManager& operator=(const AdcManager&) = delete;
    
    InterfaceStatus startAdcDma(AdcInstance adc_instance);
    InterfaceStatus stopAdcDma(AdcInstance adc_instance);
    InterfaceStatus configureAdcChannels(AdcInstance adc_instance);
    void processDmaData(AdcInstance adc_instance);
    
    static AdcInstance getAdcIndexFromHandle(CommAdcHandle* hadc);
    static void setBit(uint32_t& flags, uint32_t bit);

public:
    static AdcManager* getInstance();
    
    InterfaceStatus initialize(CommAdcHandle* hadc1, CommAdcHandle* hadc2, CommAdcHandle* hadc3,
                              CommDmaHandle* hdma_adc1, CommDmaHandle* hdma_adc2, CommDmaHandle* hdma_adc3);
    InterfaceStatus startAllAdcs();
    InterfaceStatus stopAllAdcs();
    
    InterfaceStatus getAdcData(AdcInstance adc_instance, AdcBufferData& data);
    InterfaceStatus getChannelValue(AdcInstance adc_instance, uint8_t channel_index, uint16_t& raw_value);
    InterfaceStatus getChannelVoltage(AdcInstance adc_instance, uint8_t channel_index, float& voltage);
    
    void handleDmaComplete(AdcInstance adc_instance);
    void handleDmaError(AdcInstance adc_instance);
    static void interruptContextHandler(CommAdcHandle* hadc);
    CommAdcHandle* getAdcHandle(AdcInstance adc_instance) const;
    static void processInterruptFlags();
};

#endif // HAL_ADC_MODULE_ENABLED

#endif /* INTERFACE_ADC_MANAGER_HPP_ */
