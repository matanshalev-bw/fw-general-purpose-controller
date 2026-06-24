/*
 * system_interface.cpp
 *
 *  Created on: Nov 28, 2024
 *      Author: matan
 */

#include "system_interface.hpp"
#include "comm_interface.hpp"

volatile uint32_t __attribute__((section(".ram_vector,\"aw\",%nobits @"))) ram_vector[VECTOR_TABLE_SIZE];

uint32_t SystemInterface::getTick() { return HAL_GetTick(); }

void SystemInterface::delay(const uint32_t delay_ms) {
	HAL_Delay(delay_ms);
}

InterfaceStatus SystemInterface::resetController() {
  NVIC_SystemReset();
  return InterfaceStatus::INTERFACE_ERROR;
}

InterfaceStatus SystemInterface::setVectorTable() {
//	// Enable SYSCFG clock
//	__HAL_RCC_SYSCFG_CLK_ENABLE();
//
//	// Copy vector table to RAM
//	for (uint32_t i = 0; i < VECTOR_TABLE_SIZE; i++) {
//		ram_vector[i] = g_pfnVectors[i];
//	}
//
//	// For STM32G4, remap memory to SRAM using SYSCFG MEMRMP register
//	// Set MEM_MODE to 0x03 to remap system memory to SRAM
//	SYSCFG->MEMRMP = (SYSCFG->MEMRMP & ~SYSCFG_MEMRMP_MEM_MODE) | SYSCFG_MEMRMP_MEM_MODE_1 | SYSCFG_MEMRMP_MEM_MODE_0;

	SCB->VTOR = FLASH_APPLICATION_ADDRESS;

	return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus SystemInterface::hardFault() {
  delay(200);
  resetController();
  return InterfaceStatus::INTERFACE_ERROR;
}

InterfaceStatus SystemInterface::moveToBootloader(CommCan* comm_can) {
	if (comm_can != nullptr) {
		comm_can->prepareForBootloader();
	}

	__set_MSP(*(uint32_t *)FLASH_BW_BOOTLOADER_ADDRESS);
	return resetController();
}
