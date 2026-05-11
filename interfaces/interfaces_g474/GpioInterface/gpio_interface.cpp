/*
 * gpio_interface.cpp
 *
 * Created on: Jan 16, 2025
 * Author: ariel
 */

#include "gpio_interface.hpp"
#include "adc_manager.hpp"

GPIO_TypeDef* GpioInterface::getGpioPort(GpioPortType port) {
    switch (port) {
        case GpioPortType::PORT_A: return GPIOA;
        case GpioPortType::PORT_B: return GPIOB;
        case GpioPortType::PORT_C: return GPIOC;
        case GpioPortType::PORT_F: return GPIOF;
#ifdef GPIOD
        case GpioPortType::PORT_D: return GPIOD;
#endif
#ifdef GPIOE
        case GpioPortType::PORT_E: return GPIOE;
#endif
        default: return nullptr;
    }
}

GpioPortType GpioInterface::getPortType(GPIO_TypeDef* gpio_port) {
    if (gpio_port == GPIOA) return GpioPortType::PORT_A;
    if (gpio_port == GPIOB) return GpioPortType::PORT_B;
    if (gpio_port == GPIOC) return GpioPortType::PORT_C;
    if (gpio_port == GPIOF) return GpioPortType::PORT_F;
#ifdef GPIOD
    if (gpio_port == GPIOD) return GpioPortType::PORT_D;
#endif
#ifdef GPIOE
    if (gpio_port == GPIOE) return GpioPortType::PORT_E;
#endif
    return GpioPortType::UNKNOWN_PORT;
}

GpioPin GpioInterface::createDigitalGpio(GpioPortType port, GpioPinNumber pin) {
    return GpioPin(port, static_cast<uint16_t>(pin));
}

GpioPin GpioInterface::createDigitalGpio(GPIO_TypeDef* gpio_port, uint16_t hal_pin) {
    return GpioPin(toGpioPortType(gpio_port), static_cast<uint16_t>(toGpioPinNumber(hal_pin)));
}

#ifdef HAL_ADC_MODULE_ENABLED
AnalogGpio GpioInterface::createAnalogGpio(uint32_t adc_channel, float voltage_divider_ratio, ADC_HandleTypeDef* hadc) {
    return AnalogGpio(adc_channel, voltage_divider_ratio, hadc);
}
#endif

InterfaceStatus GpioInterface::digitalWrite(const GpioPin& gpio_pin, GpioPinState state) {
    if (not gpio_pin.isValid()) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    GPIO_TypeDef* gpio_port = getGpioPort(gpio_pin.port);
    if (gpio_port == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    HAL_GPIO_WritePin(gpio_port, gpio_pin.pin, static_cast<GPIO_PinState>(state));
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus GpioInterface::digitalRead(const GpioPin& gpio_pin, GpioPinState& state) {
    if (not gpio_pin.isValid()) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    GPIO_TypeDef* gpio_port = getGpioPort(gpio_pin.port);
    if (gpio_port == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    state = static_cast<GpioPinState>(HAL_GPIO_ReadPin(gpio_port, gpio_pin.pin));
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus GpioInterface::digitalToggle(const GpioPin& gpio_pin) {
    if (not gpio_pin.isValid()) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    GPIO_TypeDef* gpio_port = getGpioPort(gpio_pin.port);
    if (gpio_port == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    HAL_GPIO_TogglePin(gpio_port, gpio_pin.pin);
    return InterfaceStatus::INTERFACE_OK;
}

#ifdef HAL_ADC_MODULE_ENABLED
InterfaceStatus GpioInterface::analogRead(const AnalogGpio& analog_gpio, float& voltage) {
    ADC_HandleTypeDef* hadc = analog_gpio.adc_handler;
    if (hadc == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    ADC_ChannelConfTypeDef sConfig = {0};
    
    __HAL_ADC_CLEAR_FLAG(hadc, (ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR));

    sConfig.Channel = analog_gpio.channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
    
    if (HAL_ADC_ConfigChannel(hadc, &sConfig) != HAL_OK) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    if (HAL_ADC_Start(hadc) != HAL_OK) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    if (HAL_ADC_PollForConversion(hadc, 100) == HAL_OK) {
        uint16_t raw_value = HAL_ADC_GetValue(hadc);
        voltage = adcToVoltage(raw_value, analog_gpio.voltage_divider_ratio, hadc);
    } else {
        HAL_ADC_Stop(hadc);
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    HAL_ADC_Stop(hadc);
    return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus GpioInterface::calibrateAdc(ADC_HandleTypeDef* hadc) {
    if (hadc == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    if (HAL_ADC_Stop(hadc) != HAL_OK) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    // For G474, specify single-ended input calibration
    if (HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED) != HAL_OK) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    while (HAL_ADC_GetState(hadc) == HAL_ADC_STATE_BUSY_INTERNAL) {
    }
    
    if (HAL_ADC_GetState(hadc) != HAL_ADC_STATE_READY) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    return InterfaceStatus::INTERFACE_OK;
}

float GpioInterface::adcToVoltage(uint16_t adc_value, float voltage_divider_ratio, ADC_HandleTypeDef* hadc) {
    return static_cast<float>(__LL_ADC_CALC_DATA_TO_VOLTAGE(VREFINT_CAL_VREF,
                                                            adc_value,
                                                            hadc->Init.Resolution)) / (1000.0f * voltage_divider_ratio);
}

InterfaceStatus GpioInterface::analogReadDma(AdcInstance adc_instance, uint8_t channel_index, float& voltage) {
    AdcManager* buffer_manager = AdcManager::getInstance();
    if (buffer_manager == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    InterfaceStatus status = buffer_manager->getChannelVoltage(adc_instance, channel_index, voltage);
    if (status != InterfaceStatus::INTERFACE_OK) {
        voltage = 0.0f;  // Set default value on error
    }
    return status;
}

InterfaceStatus GpioInterface::initializeAdcDma(ADC_HandleTypeDef* hadc1, ADC_HandleTypeDef* hadc2, ADC_HandleTypeDef* hadc3,
                                             DMA_HandleTypeDef* hdma_adc1, DMA_HandleTypeDef* hdma_adc2, DMA_HandleTypeDef* hdma_adc3) {
    AdcManager* buffer_manager = AdcManager::getInstance();
    return buffer_manager->initialize(hadc1, hadc2, hadc3, hdma_adc1, hdma_adc2, hdma_adc3);
}

InterfaceStatus GpioInterface::startAdcDma() {
    AdcManager* buffer_manager = AdcManager::getInstance();
    if (buffer_manager == nullptr) {
        return InterfaceStatus::INTERFACE_ERROR;
    }
    
    // Calibrate all ADCs before starting DMA
    ADC_HandleTypeDef* adc_handles[] = {buffer_manager->getAdcHandle(AdcInstance::ADC_1), 
                                       buffer_manager->getAdcHandle(AdcInstance::ADC_2), 
                                       buffer_manager->getAdcHandle(AdcInstance::ADC_3)};
    
    for (uint8_t i = 0; i < 3; i++) {
        if (adc_handles[i] != nullptr) {
            InterfaceStatus status = calibrateAdc(adc_handles[i]);
            if (status != InterfaceStatus::INTERFACE_OK) {
                return status;
            }
        }
    }
    
    return buffer_manager->startAllAdcs();
}

InterfaceStatus GpioInterface::stopAdcDma() {
    AdcManager* buffer_manager = AdcManager::getInstance();
    return buffer_manager->stopAllAdcs();
}

extern "C" {
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    AdcManager::interruptContextHandler(hadc);
}
}
#endif
