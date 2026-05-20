#include "non_volatile_memory_interface.hpp"

volatile const FLASH_CONFIG_SECTION ConfigMemory NonVolatileMemoryInterface::CONFIG_MEMORY_{};
volatile const FLASH_META_DATA_SECTION MetaData NonVolatileMemoryInterface::META_DATA_{};
bool NonVolatileMemoryInterface::is_config_memory_valid = false;

inline bool NonVolatileMemoryInterface::isConfigMemoryExist() {
	return (memcmp(const_cast<const char*>(CONFIG_MEMORY_.START_SIGN),
			CONFIGS_START_SIGN, sizeof(CONFIGS_START_SIGN)) == 0);
}

inline bool NonVolatileMemoryInterface::isConfigMemoryVersionOk() {
	const ConfigMemoryVersion config_version{};
	return (memcmp(const_cast<const ConfigMemoryVersion*>(
			&CONFIG_MEMORY_.VERSION),
			&config_version, sizeof(config_version)) == 0);
}

inline bool NonVolatileMemoryInterface::isConfigMemoryFullyPresent() {
	return (memcmp(const_cast<const char*>(CONFIG_MEMORY_.END_SIGN),
			CONFIGS_END_SIGN, sizeof(CONFIGS_END_SIGN)) == 0);
}


bool NonVolatileMemoryInterface::isConfigMemoryValid() {
	if (is_config_memory_valid) {
		return true;
	}
	if (not isConfigMemoryExist() or not isConfigMemoryVersionOk() or not isConfigMemoryFullyPresent()) {
		return false;
	}
	is_config_memory_valid = true;
	return true;
}

InterfaceStatus NonVolatileMemoryInterface::writeDataToFlash(const void* data, size_t size) {

	// Prepare to erase the FLASH page
	const uint32_t nbPages = (FLASH_META_DATA_BYTES_SIZE + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE; // Round up division

	// Calculate page number from address
	uint32_t page_num = ((uint32_t)&NonVolatileMemoryInterface::META_DATA_ - FLASH_BASE) / FLASH_PAGE_SIZE;
	
	FLASH_EraseInitTypeDef EraseInitStruct = {0};
	EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
	EraseInitStruct.Banks = FLASH_BANK_1;
	EraseInitStruct.Page = page_num;
	EraseInitStruct.NbPages = nbPages;
	uint32_t page_error;

	// Flash operations
	InterfaceStatus status;
	status = static_cast<InterfaceStatus>(HAL_FLASH_Unlock());
	if (status != InterfaceStatus::INTERFACE_OK) {
		// Handle unlock error
		return InterfaceStatus::INTERFACE_ERROR;
	}

	// Erase the page
	status = static_cast<InterfaceStatus>(HAL_FLASHEx_Erase(&EraseInitStruct, &page_error));
	if (status != InterfaceStatus::INTERFACE_OK) {
		// Handle erase error
		HAL_FLASH_Lock();
		return InterfaceStatus::INTERFACE_ERROR;
	}

	// Program the data double-word by double-word (G474 requires 64-bit programming)
	const uint64_t* src_ptr = reinterpret_cast<const uint64_t*>(data);
	uint32_t dest_addr = reinterpret_cast<uint32_t>(&NonVolatileMemoryInterface::META_DATA_);
	const uint32_t dword_count = (size + sizeof(uint64_t) - 1) / sizeof(uint64_t); // Round up division

	for (size_t i = 0; i < dword_count; i++) {
		uint64_t data_to_write = (i * sizeof(uint64_t) < size) ? src_ptr[i] : 0; // Pad with zeros if needed
		status = static_cast<InterfaceStatus>(HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, dest_addr + (i*sizeof(uint64_t)), data_to_write));

		if (status != InterfaceStatus::INTERFACE_OK) {
			// Handle programming error
			HAL_FLASH_Lock();
			return InterfaceStatus::INTERFACE_ERROR;
		}
	}

	// Lock the FLASH when done
	HAL_FLASH_Lock();
	return InterfaceStatus::INTERFACE_OK;
}

// Flash operations for bootloader

uint32_t NonVolatileMemoryInterface::getPage(uint32_t addr)
{
    uint32_t page = 0;

    if (addr < (FLASH_BASE + FLASH_BANK_SIZE))
    {
        /* Bank 1 */
        page = (addr - FLASH_BASE) / FLASH_PAGE_SIZE;
    }
    else
    {
        /* Bank 2 */
        page = (addr - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE;
    }

    return page;
}



InterfaceStatus NonVolatileMemoryInterface::eraseFlash(uint32_t start_address, uint32_t size) {
	// Calculate end address
	const uint32_t end_address = start_address + size;
	
	// Check if we need to erase across both banks
	const uint32_t bank1_end = FLASH_BASE + FLASH_BANK_SIZE; // 0x08040000
	
	if (start_address < bank1_end && end_address > bank1_end) {
		// Erase spans both banks - need to erase in two operations
		
		// Calculate sizes for each bank
		const uint32_t bank1_size = bank1_end - start_address;
		const uint32_t bank2_size = end_address - bank1_end;
		
		// Erase Bank 1 portion
		InterfaceStatus status = eraseFlashBank(start_address, bank1_size, FLASH_BANK_1);
		if (status != InterfaceStatus::INTERFACE_OK) {
			return status;
		}
		
		// Erase Bank 2 portion
		status = eraseFlashBank(bank1_end, bank2_size, FLASH_BANK_2);
		if (status != InterfaceStatus::INTERFACE_OK) {
			return status;
		}
		
		return InterfaceStatus::INTERFACE_OK;
	} else {
		// Erase is within a single bank
		const uint32_t bank = (start_address < bank1_end) ? FLASH_BANK_1 : FLASH_BANK_2;
		return eraseFlashBank(start_address, size, bank);
	}
}

InterfaceStatus NonVolatileMemoryInterface::flashProgram(uint32_t address, uint64_t data) {
	return static_cast<InterfaceStatus>(HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data));
}

InterfaceStatus NonVolatileMemoryInterface::eraseFlashBank(uint32_t start_address, uint32_t size, uint32_t bank) {
	// Get the page number for the given address and bank
	uint32_t page;
	if (bank == FLASH_BANK_1) {
		page = (start_address - FLASH_BASE) / FLASH_PAGE_SIZE;
	} else {
		page = (start_address - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE;
	}

	// Calculate how many pages we need
	const uint32_t nb_pages = (size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE; // Round up division

	// Prepare flash erase structure
	FLASH_EraseInitTypeDef EraseInitStruct;
	EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
	EraseInitStruct.Banks = bank;
	EraseInitStruct.Page = page;
	EraseInitStruct.NbPages = nb_pages;

	uint32_t page_error = 0;

	// Unlock the Flash to enable the flash control register access
	InterfaceStatus unlock_flash_status = unlockFlash();
	if (unlock_flash_status != InterfaceStatus::INTERFACE_OK) {
		return InterfaceStatus::INTERFACE_ERROR;
	}

	// Clear all flash error flags before proceeding
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

	// Erase the page
	InterfaceStatus erase_flash_status = static_cast<InterfaceStatus>(HAL_FLASHEx_Erase(&EraseInitStruct, &page_error));
	if (erase_flash_status != InterfaceStatus::INTERFACE_OK) {
		HAL_FLASH_Lock();
		return InterfaceStatus::INTERFACE_ERROR;
	}
	return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus NonVolatileMemoryInterface::unlockFlash(bool unlock) {
	if (unlock) {
		HAL_FLASH_Unlock();
	} else {
		HAL_FLASH_Lock();
	}
	return InterfaceStatus::INTERFACE_OK;
}

InterfaceStatus NonVolatileMemoryInterface::rewriteMetaData() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
	MetaData updated_meta_data{};
	if (isConfigMemoryValid()) {
		memcpy(&updated_meta_data.config_type, const_cast<const ConfigType*>(&CONFIG_MEMORY_.config_type), sizeof(CONFIG_MEMORY_.config_type));
	}
	return writeDataToFlash(&updated_meta_data, sizeof(MetaData));
#pragma GCC diagnostic pop
}

InterfaceStatus NonVolatileMemoryInterface::updateProgrammingStateOnMetaData(const ProgrammingState new_state) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
	// Create a local copy of the configuration
	MetaData updated_meta_data{};
	memcpy(&updated_meta_data, const_cast<const MetaData*>(&NonVolatileMemoryInterface::META_DATA_), sizeof(MetaData));

	// Update the programming state
	updated_meta_data.programming_state = new_state;

	// Write the updated data to flash
	return writeDataToFlash(&updated_meta_data, sizeof(MetaData));
#pragma GCC diagnostic pop
}

InterfaceStatus NonVolatileMemoryInterface::updateBootloaderVersionOnMetaData() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
	const BootloaderVersion current_version{};
	const volatile MetaData& meta_data = NonVolatileMemoryInterface::META_DATA_;
	if (meta_data.BOOTLOADER_VERSION.major == current_version.major and
	    meta_data.BOOTLOADER_VERSION.minor == current_version.minor and
	    meta_data.BOOTLOADER_VERSION.patch == current_version.patch) {
		return InterfaceStatus::INTERFACE_OK;
	}

	MetaData updated_meta_data{};
	memcpy(&updated_meta_data, const_cast<const MetaData*>(&meta_data), sizeof(MetaData));
	updated_meta_data.BOOTLOADER_VERSION = current_version;

	return writeDataToFlash(&updated_meta_data, sizeof(MetaData));
#pragma GCC diagnostic pop
}

