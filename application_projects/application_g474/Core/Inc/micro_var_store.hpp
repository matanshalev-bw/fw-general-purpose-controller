#ifndef MICRO_VAR_STORE_HPP_
#define MICRO_VAR_STORE_HPP_

#include <cstdint>
#include "sequences_configs.hpp"

class MicroVarStore {
 public:
  uint32_t get(uint8_t index) const;
  void set(uint8_t index, uint32_t value);
  void clearAll();

 private:
  uint32_t vars_[MICRO_VAR_SLOT_COUNT] = {};
};

#endif  // MICRO_VAR_STORE_HPP_
