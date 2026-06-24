#include "application_main.hpp"

#include <memory>

#include "bluewhite_can_comm.hpp"
#include "bluewhite_usb_comm.hpp"
#include "gpc_controller.hpp"
#include "gpc_telemetry_sender.hpp"
#include "main.h"
#include "micro_sequence_executor.hpp"
#include "micro_var_store.hpp"
#include "non_volatile_memory_interface.hpp"
#include "raw_can_interface.hpp"

#ifdef HAL_ADC_MODULE_ENABLED
#include "adc_manager.hpp"
#include "comm_defines.hpp"
#endif

extern CommCanHandle hfdcan2;
extern CommCanHandle hfdcan3;

namespace {
std::unique_ptr<MicroVarStore> g_var_store;
std::unique_ptr<MicroSequenceExecutor> g_sequence_executor;
std::unique_ptr<GpcController> g_gpc_controller;
std::unique_ptr<RawCanInterface> g_raw_can;
std::unique_ptr<BluewhiteCanComm> g_bluewhite_can;
std::unique_ptr<BluewhiteUsbComm> g_bluewhite_usb;
std::unique_ptr<GpcTelemetrySender> g_telemetry_sender;
}  // namespace

void applicationInit(void) {
  const bool config_valid = NonVolatileMemoryInterface::isConfigMemoryValid();
  NonVolatileMemoryInterface::rewriteMetaData();

  g_var_store = std::make_unique<MicroVarStore>();
  g_var_store->clearAll();

  g_sequence_executor = std::make_unique<MicroSequenceExecutor>();
  g_gpc_controller = std::make_unique<GpcController>();
  g_raw_can = std::make_unique<RawCanInterface>(&hfdcan3);
  g_sequence_executor->setRawCanInterface(g_raw_can.get());
  g_sequence_executor->setVarStore(g_var_store.get());
  g_gpc_controller->setRawCanInterface(g_raw_can.get());
  g_gpc_controller->setVarStore(g_var_store.get());

  if (config_valid) {
    const volatile MicroSequence& powerup =
        NonVolatileMemoryInterface::CONFIG_MEMORY_.sequences_config.powerup_sequence;
    if (powerup.step_count > 0) {
      g_sequence_executor->start(powerup);
    }
  }

#ifdef HAL_ADC_MODULE_ENABLED
  extern CommAdcHandle hadc1;
  extern CommAdcHandle hadc2;
  extern CommDmaHandle hdma_adc1;
  extern CommDmaHandle hdma_adc2;
  if (AdcManager::getInstance()->initialize(&hadc1, &hadc2, nullptr, &hdma_adc1, &hdma_adc2, nullptr) ==
      InterfaceStatus::INTERFACE_OK) {
    AdcManager::getInstance()->startAllAdcs();
  }
#endif

  g_bluewhite_can =
      std::make_unique<BluewhiteCanComm>(&hfdcan2, g_sequence_executor.get(), g_gpc_controller.get());
  g_bluewhite_usb = std::make_unique<BluewhiteUsbComm>(g_sequence_executor.get(), g_bluewhite_can->bootloaderComm(),
                                                      g_gpc_controller.get());
  g_bluewhite_usb->initialize();

  g_telemetry_sender = std::make_unique<GpcTelemetrySender>();
  g_telemetry_sender->initialize(g_var_store.get(), g_bluewhite_can.get(), g_bluewhite_usb.get());
}

void applicationTick(void) {
  if (g_gpc_controller) {
    g_gpc_controller->tick();
  }
  if (g_telemetry_sender) {
    g_telemetry_sender->tick();
  }
  if (g_bluewhite_can) {
    g_bluewhite_can->tick();
  }
  if (g_bluewhite_usb) {
    g_bluewhite_usb->tick();
  }
}
