#include "micro_var_store.hpp"

#include <cstring>

uint32_t MicroVarStore::get(uint8_t index) const {
  if (index >= MICRO_VAR_SLOT_COUNT) {
    return 0;
  }
  return vars_[index];
}

void MicroVarStore::set(uint8_t index, uint32_t value) {
  if (index >= MICRO_VAR_SLOT_COUNT) {
    return;
  }
  vars_[index] = value;
}

void MicroVarStore::clearAll() { memset(vars_, 0, sizeof(vars_)); }
