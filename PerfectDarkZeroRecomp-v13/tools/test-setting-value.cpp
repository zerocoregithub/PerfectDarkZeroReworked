#include "../src/pdz_setting_value.h"
#include <cassert>
#include <iostream>
int main() {
  std::string out;
  for(int i=-1;i<=15;++i) {
    toml::value<int64_t> node(i);
    assert(pdz::settings::RuntimeValue(node,out));
    assert(out==std::to_string(i));
    // Reproduce the original conversion bug for the reported A-button index.
    if(i==10)assert(static_cast<const toml::node&>(node).value<bool>().value()==true);
  }
  toml::value<bool> on(true),off(false);
  assert(pdz::settings::RuntimeValue(on,out)&&out=="true");
  assert(pdz::settings::RuntimeValue(off,out)&&out=="false");
  toml::value<double> decimal(1.6),whole(2.0);
  assert(pdz::settings::RuntimeValue(decimal,out)&&std::stod(out)==1.6);
  assert(pdz::settings::RuntimeValue(whole,out)&&std::stod(out)==2.0);
  toml::value<std::string> text("10");
  assert(pdz::settings::RuntimeValue(text,out)&&out=="10");
  toml::array array;assert(!pdz::settings::RuntimeValue(array,out));
  std::cout<<"PASS: button indices, booleans, fractional/integral doubles, strings and invalid-node rejection\n";
}
