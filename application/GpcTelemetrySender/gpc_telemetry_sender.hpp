#ifndef GPC_TELEMETRY_SENDER_HPP_
#define GPC_TELEMETRY_SENDER_HPP_

#include <cstdint>
#include <memory>

#include "scheduler_interface.hpp"
#include "sequences_configs.hpp"

class BluewhiteCanComm;
class BluewhiteUsbComm;
class GpcController;
class MicroVarStore;

class GpcTelemetrySender {
 public:
  void initialize(MicroVarStore* var_store, BluewhiteCanComm* can_comm, BluewhiteUsbComm* usb_comm,
                  GpcController* gpc_controller);
  void tick();

 private:
  static constexpr uint8_t CONTROLLER_STATE_TELEMETRY_RATE_HZ = 5;

  void buildPayload(const volatile TelemetryBinding& binding, uint8_t* out) const;
  bool sendBinding(const volatile TelemetryBinding& binding, const uint8_t* payload);
  void tickControllerStateTelemetry();

  MicroVarStore* var_store_ = nullptr;
  BluewhiteCanComm* can_comm_ = nullptr;
  BluewhiteUsbComm* usb_comm_ = nullptr;
  GpcController* gpc_controller_ = nullptr;
  std::unique_ptr<SchedulerMainClock> schedulers_[MAX_TELEMETRY_BINDINGS];
  std::unique_ptr<SchedulerMainClock> controller_state_scheduler_;
};

#endif  // GPC_TELEMETRY_SENDER_HPP_
