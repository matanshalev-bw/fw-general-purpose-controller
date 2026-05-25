#include "application_main.hpp"

#include <memory>

#include "bluewhite_can_comm.hpp"
#include "bluewhite_usb_comm.hpp"
#include "main.h"
#include "micro_sequence_executor.hpp"
#include "non_volatile_memory_interface.hpp"
#include "raw_can_interface.hpp"

#ifdef HAL_ADC_MODULE_ENABLED
#include "adc_manager.hpp"
#endif

extern FDCAN_HandleTypeDef hfdcan2;
extern FDCAN_HandleTypeDef hfdcan3;

namespace {
std::unique_ptr<MicroSequenceExecutor> g_sequence_executor;
std::unique_ptr<RawCanInterface> g_raw_can;
std::unique_ptr<BluewhiteCanComm> g_bluewhite_can;
std::unique_ptr<BluewhiteUsbComm> g_bluewhite_usb;
}  // namespace

extern "C" void applicationInit(void) {
  if (not NonVolatileMemoryInterface::isConfigMemoryValid()) {
    Error_Handler();
  }

  NonVolatileMemoryInterface::rewriteMetaData();

  g_sequence_executor = std::make_unique<MicroSequenceExecutor>();
  g_raw_can = std::make_unique<RawCanInterface>(&hfdcan3);
  g_sequence_executor->setRawCanInterface(g_raw_can.get());

  const volatile MicroSequence& powerup =
      NonVolatileMemoryInterface::CONFIG_MEMORY_.sequences_config.powerup_sequence;
  if (powerup.step_count > 0) {
    g_sequence_executor->start(powerup);
  }

#ifdef HAL_ADC_MODULE_ENABLED
  extern ADC_HandleTypeDef hadc1;
  extern ADC_HandleTypeDef hadc2;
  extern DMA_HandleTypeDef hdma_adc1;
  extern DMA_HandleTypeDef hdma_adc2;
  if (AdcManager::getInstance()->initialize(&hadc1, &hadc2, nullptr, &hdma_adc1, &hdma_adc2, nullptr) ==
      InterfaceStatus::INTERFACE_OK) {
    AdcManager::getInstance()->startAllAdcs();
  }
#endif

  g_bluewhite_can = std::make_unique<BluewhiteCanComm>(&hfdcan2, g_sequence_executor.get());
  g_bluewhite_usb = std::make_unique<BluewhiteUsbComm>(g_sequence_executor.get(), g_bluewhite_can->bootloaderComm());
  g_bluewhite_usb->initialize();
}

extern "C" void applicationTick(void) {
  if (g_bluewhite_can) {
    g_bluewhite_can->tick();
  }
  if (g_bluewhite_usb) {
    g_bluewhite_usb->tick();
  }
}
