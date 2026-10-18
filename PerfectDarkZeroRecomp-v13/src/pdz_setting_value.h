#pragma once
#include <toml++/toml.hpp>
#include <iomanip>
#include <sstream>
#include <string>

namespace pdz::settings {
inline bool RuntimeValue(const toml::node& node,std::string& value) {
  // Conversion-enabled value<bool>() accepts integers: a button index would
  // become "true" and fail the integer cvar setter. Preserve the TOML type.
  if(auto v=node.value_exact<std::string>()) value=*v;
  else if(auto v=node.value_exact<bool>()) value=*v?"true":"false";
  else if(auto v=node.value_exact<int64_t>()) value=std::to_string(*v);
  else if(auto v=node.value_exact<double>()) {
    std::ostringstream text;text<<std::setprecision(17)<<*v;value=text.str();
  } else return false;
  return true;
}
}
