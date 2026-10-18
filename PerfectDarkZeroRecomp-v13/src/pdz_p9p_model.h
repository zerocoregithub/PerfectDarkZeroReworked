#pragma once
#include <cstdint>
void PDZ_UpdateP9PModel();
class PDZ_P9PModelScope {
 public:
  PDZ_P9PModelScope(uint8_t* base,uint32_t descriptor,uint32_t instance,uint32_t caller);
  ~PDZ_P9PModelScope();
  PDZ_P9PModelScope(const PDZ_P9PModelScope&)=delete;
  PDZ_P9PModelScope& operator=(const PDZ_P9PModelScope&)=delete;
 private:bool previous_;
};
bool PDZ_P9PGroupHidden(uint32_t group);
