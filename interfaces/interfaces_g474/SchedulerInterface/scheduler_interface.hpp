/*
 * timerinterface.hpp
 *
 *  Created on: Jul 10, 2024
 *      Author: matan
 */

#ifndef SRC_SCHEDULER_INTERFACE_HPP_
#define SRC_SCHEDULER_INTERFACE_HPP_

#include "interface_status.hpp"

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_tim.h"

class SchedulerInterface {
 public:
  virtual bool isDue() const { return false; }
  virtual InterfaceStatus restart() { return InterfaceStatus::INTERFACE_ERROR; }
};

///////////////////////////////// TIMER /////////////////////////////////

class SchedulerTimer : public SchedulerInterface {
 protected:
  TIM_HandleTypeDef *timer_ = nullptr;
  bool *timer_flag_ = nullptr;

  InterfaceStatus setTimerPeriod(TIM_HandleTypeDef *timer, const float freq);

 public:
  SchedulerTimer(TIM_HandleTypeDef *timer, const float freq, bool *timer_flag);
  bool isDue() const override;
  InterfaceStatus restart() override;
};

//////////////////////////////// TIMER ONCE /////////////////////////////

class SchedulerTimerOnce : public SchedulerTimer {
 public:
  SchedulerTimerOnce(TIM_HandleTypeDef *timer, bool *timer_flag) : SchedulerTimer(timer, 0, timer_flag) {}
  InterfaceStatus reset(const uint32_t millis);
};

//////////////////////////////// MAIN CLOCK /////////////////////////////

class SchedulerMainClock : public SchedulerInterface {
 private:
  const uint32_t PERIOD_;
  uint32_t last_time_ = 0;

 public:
  SchedulerMainClock(const float freq) : PERIOD_(1000 / freq) { last_time_ = HAL_GetTick(); }
  bool isDue() const override;
  uint32_t timeElapsed() const;
  InterfaceStatus restart() override;
};

////////////////////////////// MAIN CLOCK ONCE ///////////////////////////

class SchedulerOnce : public SchedulerInterface {
 private:
  uint32_t period_ = 0;
  bool is_finished_ = false;
  uint32_t last_time_ = 0;

 public:
  SchedulerOnce(const uint32_t millis) : period_(millis) { last_time_ = HAL_GetTick(); }
  bool isDue();
  bool isActive() const;
  uint32_t timeElapsed() const;
  InterfaceStatus reset(const uint32_t millis);
  InterfaceStatus restart() override;
};

#endif /* SRC_TIMERINTERFACE_HPP_ */
