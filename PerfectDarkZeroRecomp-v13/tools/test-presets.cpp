#include "../src/pdz_presets.h"
#include "preset-policy-reference.h"
#include <cassert>
#include <cmath>
#include <set>
#include <string>
#include <iostream>
#include <iterator>
int main(){
 using namespace pdz::settings;
 constexpr Preset policies[]={Preset::Classic,Preset::GameCube,Preset::Modernized,Preset::Vanilla};
 static_assert(std::size(test::kReference)==std::size(kPresetValues));
 std::set<std::string> names;
 std::size_t row=0;
 for(const auto& v:kPresetValues){
   const auto& reference=test::kReference[row++];
   assert(std::string(v.name)==reference.name);
   for(std::size_t i=0;i<std::size(policies);++i)
     assert(std::string(Value(v,policies[i]))==reference.values[i]);
   assert(names.insert(v.name).second);
   const std::string name=v.name;
   assert(name.find("pdz_mouse_")!=0&&(name.find("pdz_stick_")!=0||name=="pdz_stick_curve")&&name.find("pdz_pad_")!=0&&name.find("keybind_")!=0);
   if(name=="pdz_stick_curve") {
     assert(std::string(Value(v,Preset::Classic))=="relaxed");
     assert(std::string(Value(v,Preset::Modernized))=="relaxed");
     assert(std::string(Value(v,Preset::Vanilla))=="original");
   }
   assert(name!="pdz_direct_mouse"&&name!="pdz_controller_remap"&&name!="pdz_frame_limit"&&name!="pdz_host_vsync");
   assert(name!="pdz_auto_aim_scoped");
   if(name=="pdz_auto_aim") assert(std::string(Value(v,Preset::Vanilla))=="false"&&std::string(Value(v,Preset::Classic))=="true");
   if(name=="pdz_stylized_lighting") assert(std::string(Value(v,Preset::Classic))=="false"&&std::string(Value(v,Preset::GameCube))=="true"&&std::string(Value(v,Preset::Vanilla))=="false");
   else assert(std::string(Value(v,Preset::Classic))==Value(v,Preset::GameCube));
   if(name=="pdz_classic_weapon_sway") assert(std::string(Value(v,Preset::Classic))=="true"&&std::string(Value(v,Preset::Modernized))=="false");
   if(name=="pdz_wheel_zoom"||name=="pdz_dpad_zoom") {
     assert(std::string(Value(v,Preset::Vanilla))=="false");
     assert(std::string(Value(v,Preset::Classic))=="true");
     assert(std::string(Value(v,Preset::Modernized))=="true");
   }
   if(name=="pdz_modern_movement") assert(std::string(Value(v,Preset::Modernized))=="false"&&std::string(v.vanilla)=="false");
   const bool differs=name=="pdz_native_aim_assist"||name=="pdz_auto_aim"||
       name=="pdz_classic_weapon_sway"||name=="pdz_classic_sway_aim"||name=="pdz_sway_during_ads";
   assert((std::string(Value(v,Preset::Classic))!=Value(v,Preset::Modernized))==differs);
   if(name=="pdz_dodge_roll") assert(std::string(Value(v,Preset::Classic))=="true"&&std::string(Value(v,Preset::Modernized))=="true"&&std::string(Value(v,Preset::Vanilla))=="false");
   assert(name!="pdz_dodge_roll_strength");
   if(name=="pdz_cover_camera_stabilization") assert(std::string(Value(v,Preset::Classic))=="false"&&std::string(Value(v,Preset::Modernized))=="false"&&std::string(Value(v,Preset::Vanilla))=="false");
   if(name=="pdz_horizontal_fov") {
     const double horizontal=std::stod(v.vanilla)*3.141592653589793/180;
     assert(std::abs(2*std::atan(std::tan(horizontal/2)/(16./9))*180/3.141592653589793-58)<.00001);
   }
 }
 assert(AllowAimAssist(true,false));assert(!AllowAimAssist(true,true));
 assert(!AllowAimAssist(false,true));assert(!AllowAimAssist(false,false));
 std::cout<<"PASS: 252 frozen effective preset values, schema, preserved input/frame settings, native FOV and permanent scoped assist ban\n";
}
