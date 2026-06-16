#ifndef GPC_TELEMETRY_SENDER_HPP_
#define GPC_TELEMETRY_SENDER_HPP_

#include <cstdint>
#include <memory>

#include "scheduler_interface.hpp"
#include "sequences_configs.hpp"

class BluewhiteCanComm;
class BluewhiteUsbComm;
class MicroVarStore;

class GpcTelemetrySender {
 public:
  void initialize(MicroVarStore* var_store, BluewhiteCanComm* can_comm, BluewhiteUsbComm* usb_comm);
  void tick();

 private:
  void buildPayload(const volatile TelemetryBinding& binding, uint8_t* out) const;
  bool sendBinding(const volatile TelemetryBinding& binding, const uint8_t* payload);

  MicroVarStore* var_store_ = nullptr;
  BluewhiteCanComm* can_comm_ = nullptr;
  BluewhiteUsbComm* usb_comm_ = nullptr;
  std::unique_ptr<SchedulerMainClock> schedulers_[MAX_TELEMETRY_BINDINGS];
};

#endif  // GPC_TELEMETRY_SENDER_HPP_
