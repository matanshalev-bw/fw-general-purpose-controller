#include "application_main.hpp"

#include <memory>

#include "bluewhite_can_comm.hpp"
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
}  // namespace

extern "C" void applicationInit(void) {
  if (not NonVolatileMemoryInterface::isConfigMemoryValid()) {
    Error_Handler();
  }

  g_sequence_executor = std::make_unique<MicroSequenceExecutor>();
  g_raw_can = std::make_unique<RawCanInterface>(&hfdcan3);
  g_sequence_executor->setRawCanInterface(g_raw_can.get());

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
}

extern "C" void applicationTick(void) {
  if (g_bluewhite_can) {
    g_bluewhite_can->tick();
  }
}
