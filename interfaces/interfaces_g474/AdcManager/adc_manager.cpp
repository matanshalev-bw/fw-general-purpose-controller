/*
 * adc_manager.cpp
 *
 * Created on: Jan 16, 2025
 * Author: ariel
 */

#include "adc_manager.hpp"

#ifdef HAL_ADC_MODULE_ENABLED

AdcManager* AdcManager::instance_ = nullptr;
uint32_t AdcManager::adc_interrupt_flags_ = 0;

AdcManager::AdcManager() {
    // Matches application_g474 CubeMX: PA0=ADC1_IN1, PB2=ADC2_IN12.
    static const AdcChannelConfig ADC1_CHANNELS[] = {
        AdcChannelConfig(AdcChannel::CHANNEL_1, 1, ADC_SAMPLETIME_640CYCLES_5, 1.0f),
    };
    
    static const AdcChannelConfig ADC2_CHANNELS[] = {
        AdcChannelConfig(AdcChannel::CHANNEL_12, 1, ADC_SAMPLETIME_640CYCLES_5, 1.0f),
    };
    
    static constexpr uint8_t CHANNEL_COUNTS[] = {ADC1_BUFFER_SIZE_, ADC2_BUFFER_SIZE_, ADC3_BUFFER_SIZE_};
    static constexpr const AdcChannelConfig* CHANNEL_CONFIGS[] = {
        ADC1_CHANNELS,
        ADC2_CHANNELS,
        nullptr
    };
    
    for (uint8_t i = 0; i < static_cast<uint8_t>(AdcInstance::ADC_COUNT); i++) {
        adc_configs_[i].adc_instance = static_cast<AdcInstance>(i);
        adc_configs_[i].channel_count = CHANNEL_COUNTS[i];
        
        for (uint8_t j = 0; j < CHANNEL_COUNTS[i]; j++) {
            adc_configs_[i].channels[j] = CHANNEL_CONFIGS[i][j];
        }
    }
}

AdcManager* AdcManager::getInstance() {
    if (instance_ == nullptr) {
        instance_ = new AdcManager();
    }
    return instance_;
}

InterfaceStatus AdcManager::initialize(CommAdcHandle* hadc1, CommAdcHandle* hadc2, CommAdcHandle* hadc3,
                                           CommDmaHandle* hdma_adc1, CommDmaHandle* hdma_adc2, CommDmaHandle* hdma_adc3) {
    if (is_initialized_) {
        return InterfaceStatus::INTERFACE_OK;
    }
    
    ADC_HandleTypeDef* adc_handles[] = {hadc1, hadc2, hadc3};
    DMA_HandleTypeDef* dma_handles[] = {hdma_adc1, hdma_adc2, hdma_adc3};
    
    for (uint8_t i = 0; i < static_cast<uint8_t>(AdcInstance::ADC_COUNT); i++) {
        adc_configs_[i].adc_handle = adc_handles[i];
        adc_configs_[i].dma_handle = dma_handles[i];
        
        switch (static_cast<AdcInstance>(i)) {
            case AdcInstance::ADC_1:
                adc_configs_[i].dma_buffer = dma_buffer_adc1_;
                adc_configs_[i].buffer_size = AdcManager::ADC1_BUFFER_SIZE_;
                break;
            case AdcInstance::ADC_2:
                adc_configs_[i].dma_buffer = dma_buffer_adc2_;
                adc_configs_[i].buffer_size = AdcManager::ADC2_BUFFER_SIZE_;
                break;
            case AdcInstance::ADC_3:
                adc_configs_[i].dma_buffer = nullptr;
                adc_configs_[i].buffer_size = AdcManager::ADC3_BUFFER_SIZE_;
                break;
            default:
                return InterfaceStatus::INTERFACE_ERROR;
        }
        
        if (adc_configs_[i].buffer_size != adc_configs_[i].channel_count) {
            return InterfaceStatus::INTERFACE_ERROR;
        }
    }
    
    for (uint8_t i = 0; i < static_cast<uint8_t>(AdcInstance::ADC_COUNT); i++) {
        // Skip ADCs that are not present on this board (e.g. ADC3).
        if (adc_configs_[i].adc_handle == nullptr || adc_configs_[i].channel_count == 0) {
            continue;
        }
        InterfaceStatus status = configureAdcChannels(static_cast<AdcInstance>(i));
        if (status != InterfaceStatus::INTERFACE_OK) {
            return status;
        }
    }
    
    is_initialized_ = true;
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus AdcManager::configureAdcChannels(AdcInstance adc_instance) {
    AdcDmaConfig& config = adc_configs_[static_cast<uint8_t>(adc_instance)];
    ADC_HandleTypeDef* hadc = config.adc_handle;
    
    if (hadc == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    // Channels are already configured in main.cpp; verify count matches AdcManager.
    if (hadc->Init.NbrOfConversion != config.channel_count) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus AdcManager::startAllAdcs() {
    if (not is_initialized_) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    for (uint8_t i = 0; i < static_cast<uint8_t>(AdcInstance::ADC_COUNT); i++) {
        if (adc_configs_[i].adc_handle == nullptr || adc_configs_[i].channel_count == 0) {
            continue;
        }
        InterfaceStatus status = startAdcDma(static_cast<AdcInstance>(i));
        if (status != InterfaceStatus::INTERFACE_OK) {
            return status;
        }
    }
    
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus AdcManager::startAdcDma(AdcInstance adc_instance) {
    AdcDmaConfig& config = adc_configs_[static_cast<uint8_t>(adc_instance)];
    ADC_HandleTypeDef* hadc = config.adc_handle;
    DMA_HandleTypeDef* hdma = config.dma_handle;
    
    if (hadc == nullptr or hdma == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    uint32_t total_samples = config.channel_count;
    
    if (HAL_ADC_Start_DMA(hadc, reinterpret_cast<uint32_t*>(config.dma_buffer), 
                          total_samples) != HAL_OK) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus AdcManager::stopAllAdcs() {
    for (uint8_t i = 0; i < static_cast<uint8_t>(AdcInstance::ADC_COUNT); i++) {
        if (adc_configs_[i].adc_handle == nullptr || adc_configs_[i].channel_count == 0) {
            continue;
        }
        InterfaceStatus status = stopAdcDma(static_cast<AdcInstance>(i));
        if (status != InterfaceStatus::INTERFACE_OK) {
            return status;
        }
    }
    
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus AdcManager::stopAdcDma(AdcInstance adc_instance) {
    AdcDmaConfig& config = adc_configs_[static_cast<uint8_t>(adc_instance)];
    ADC_HandleTypeDef* hadc = config.adc_handle;
    
    if (hadc == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    if (HAL_ADC_Stop_DMA(hadc) != HAL_OK) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus AdcManager::getAdcData(AdcInstance adc_instance, AdcBufferData& data) {
    if (not is_initialized_) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    processInterruptFlags();
    
    AdcBufferData& adc_data = adc_data_[static_cast<uint8_t>(adc_instance)];
    if (not adc_data.data_ready) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    data = adc_data;
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus AdcManager::getChannelValue(AdcInstance adc_instance, uint8_t channel_index, uint16_t& raw_value) {
    if (not is_initialized_ or channel_index >= MAX_ADC_CHANNELS_) {
        return InterfaceStatus::INTERFACE_ERROR;
    }

    processInterruptFlags();
    
    AdcBufferData& adc_data = adc_data_[static_cast<uint8_t>(adc_instance)];
    if (not adc_data.data_ready or channel_index >= adc_data.channel_count) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    raw_value = adc_data.raw_values[channel_index];
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus AdcManager::getChannelVoltage(AdcInstance adc_instance, uint8_t channel_index, float& voltage) {
    uint16_t raw_value;
    InterfaceStatus status = getChannelValue(adc_instance, channel_index, raw_value);
    if (status != InterfaceStatus::INTERFACE_OK) {
        return status;
    }
    
    AdcDmaConfig& config = adc_configs_[static_cast<uint8_t>(adc_instance)];
    if (channel_index >= config.channel_count || config.adc_handle == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    uint32_t voltage_mv = __LL_ADC_CALC_DATA_TO_VOLTAGE(3300,
                                                        raw_value,
                                                        config.adc_handle->Init.Resolution);
    
    voltage = static_cast<float>(voltage_mv) / (1000.0f * config.channels[channel_index].voltage_divider_ratio);
    
    return InterfaceStatus::INTERFACE_OK;
}

void AdcManager::handleDmaComplete(AdcInstance adc_instance) {
    processDmaData(adc_instance);
}

void AdcManager::handleDmaError(AdcInstance adc_instance) {
    adc_data_[static_cast<uint8_t>(adc_instance)].data_ready = false;
}

void AdcManager::processDmaData(AdcInstance adc_instance) {
    AdcDmaConfig& config = adc_configs_[static_cast<uint8_t>(adc_instance)];
    AdcBufferData& adc_data = adc_data_[static_cast<uint8_t>(adc_instance)];
    
    for (uint8_t i = 0; i < config.channel_count; i++) {
        adc_data.raw_values[i] = config.dma_buffer[i];
    }
    
    adc_data.channel_count = config.channel_count;
    adc_data.data_ready = true;
    adc_data.timestamp = HAL_GetTick();
}

CommAdcHandle* AdcManager::getAdcHandle(AdcInstance adc_instance) const {
    if (static_cast<uint8_t>(adc_instance) >= static_cast<uint8_t>(AdcInstance::ADC_COUNT)) {
        return nullptr;
    }
    return adc_configs_[static_cast<uint8_t>(adc_instance)].adc_handle;
}

void AdcManager::interruptContextHandler(CommAdcHandle* hadc) {
    uint8_t adc_index = static_cast<uint8_t>(getAdcIndexFromHandle(hadc));
    uint32_t bitmask = 1 << adc_index;
    setBit(adc_interrupt_flags_, bitmask);
}

AdcInstance AdcManager::getAdcIndexFromHandle(CommAdcHandle* hadc) {
    if (hadc->Instance == ADC1) {
        return AdcInstance::ADC_1;
    } else if (hadc->Instance == ADC2) {
        return AdcInstance::ADC_2;
    } else if (hadc->Instance == ADC3) {
        return AdcInstance::ADC_3;
    }
    return AdcInstance::ADC_1;
}

void AdcManager::setBit(uint32_t& flags, uint32_t bit) {
    flags |= bit;
}

void AdcManager::processInterruptFlags() {
    if (instance_ == nullptr) {
        return;
    }
    
    for (uint8_t i = 0; i < static_cast<uint8_t>(AdcInstance::ADC_COUNT); i++) {
        uint32_t bitmask = 1 << i;
        if (adc_interrupt_flags_ & bitmask) {
            adc_interrupt_flags_ &= ~bitmask;
            instance_->processDmaData(static_cast<AdcInstance>(i));
        }
    }
}

#endif // HAL_ADC_MODULE_ENABLED
