#include "pdz_input.h"
#include "pdz_slider.h"
#include "pdz_shoulder_swap.h"
#include "pdz_ui_mouse.h"
#include "pdz_gyro_math.h"
#include <imgui.h>
#include <rex/cvar.h>
#include <rex/input/input_system.h>
#include <rex/runtime.h>
#include <rex/logging.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ui/keybinds.h>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <string>

REXCVAR_DEFINE_STRING(pdz_stick_curve,"original","Input","Controller look response").allowed({"original","linear","relaxed","wide","extra_wide"});
REXCVAR_DEFINE_DOUBLE(pdz_stick_inner,.08,"Input","Radial look deadzone").range(0.,.4);
REXCVAR_DEFINE_DOUBLE(pdz_stick_outer,.98,"Input","Full-turn stick threshold").range(.5,1.);
REXCVAR_DEFINE_DOUBLE(pdz_stick_yaw,240.,"Input","Horizontal degrees per second").range(30.,720.);
REXCVAR_DEFINE_DOUBLE(pdz_stick_pitch,180.,"Input","Vertical degrees per second").range(30.,720.);
REXCVAR_DEFINE_DOUBLE(pdz_stick_ads,.5,"Input","ADS stick sensitivity multiplier").range(.05,2.);
REXCVAR_DEFINE_DOUBLE(pdz_stick_smoothing,0.,"Input","Optional stick easing seconds").range(0.,.15);
REXCVAR_DEFINE_DOUBLE(pdz_stick_acceleration,0.,"Input","Optional full-tilt turn boost").range(0.,1.);
REXCVAR_DEFINE_BOOL(pdz_stick_invert_y,false,"Input","Invert controller vertical look");
REXCVAR_DEFINE_BOOL(pdz_controller_remap,false,"Input","Apply controller button remapping");
REXCVAR_DEFINE_BOOL(pdz_wheel_zoom,true,"Input","Mouse wheel changes zoom stages while aiming");
REXCVAR_DEFINE_BOOL(pdz_menu_bindings,false,"Input","Separate keyboard bindings for game menus");
REXCVAR_DEFINE_BOOL(pdz_controller_menu_remap,false,"Input","Separate controller bindings for game menus");
REXCVAR_DEFINE_BOOL(pdz_stick_free_aim,false,"Input","Ignored legacy right-stick free-aim setting");
REXCVAR_DEFINE_BOOL(pdz_stick_centered_hipfire,false,"Input","Ignored legacy experimental setting");
REXCVAR_DEFINE_STRING(pdz_gyro_mode,"off","Input/Gyro","Experimental native motion aiming").allowed({"off","ads","always"});
REXCVAR_DEFINE_BOOL(pdz_gyro_steam_driver,false,"Input/Gyro","Enable native Steam Controller driver at startup (restart required)");
REXCVAR_DEFINE_DOUBLE(pdz_gyro_sensitivity,1.,"Input/Gyro","Degrees of aim per degree of controller motion").range(.1,10.);
REXCVAR_DEFINE_DOUBLE(pdz_gyro_ads,.5,"Input/Gyro","Gyro aiming sensitivity multiplier").range(.05,2.);
REXCVAR_DEFINE_STRING(pdz_gyro_acceleration,"none","Input/Gyro","Motion-speed based gyro acceleration").allowed({"none","linear","relaxed","aggressive"});
REXCVAR_DEFINE_DOUBLE(pdz_gyro_yaw_scale,1.,"Input/Gyro","Gyro horizontal multiplier").range(.1,3.);
REXCVAR_DEFINE_DOUBLE(pdz_gyro_pitch_scale,1.,"Input/Gyro","Gyro vertical multiplier").range(.1,3.);
REXCVAR_DEFINE_DOUBLE(pdz_gyro_smoothing,.015,"Input/Gyro","Gyro smoothing seconds").range(0.,.15);
REXCVAR_DEFINE_BOOL(pdz_gyro_tiered_smoothing,true,"Input/Gyro","Bypass smoothing for deliberate faster turns");
REXCVAR_DEFINE_DOUBLE(pdz_gyro_smooth_speed,10.,"Input/Gyro","Gyro speed above which smoothing is bypassed").range(1.,100.);
REXCVAR_DEFINE_BOOL(pdz_gyro_rumble_filter,false,"Input/Gyro","Experimental rumble-aware gyro smoothing");
REXCVAR_DEFINE_DOUBLE(pdz_gyro_threshold,.1,"Input/Gyro","Resting motion threshold degrees per second").range(0.,5.);
REXCVAR_DEFINE_INT32(pdz_gyro_axis,0,"Input/Gyro","Horizontal gyro axis: yaw, roll, combined").range(0,2);
REXCVAR_DEFINE_STRING(pdz_gyro_space,"local","Input/Gyro","Gyro coordinate space").allowed({"local","player","world"});
REXCVAR_DEFINE_INT32(pdz_gyro_clutch,-1,"Input/Gyro","Physical button that temporarily disables gyro").range(-1,27);
REXCVAR_DEFINE_INT32(pdz_gyro_enable_input,-1,"Input/Gyro","Hold physical extended input to enable gyro; -1 is ungated").range(-1,27);
REXCVAR_DEFINE_BOOL(pdz_extra_buttons,false,"Input/Gyro","Independent extended controller bindings");
#define EXTRA_BIND(n) REXCVAR_DEFINE_INT32(pdz_extra_##n,-1,"Input/Gyro","Extended physical input destination").range(-1,15)
EXTRA_BIND(r4); EXTRA_BIND(l4); EXTRA_BIND(r5); EXTRA_BIND(l5); EXTRA_BIND(touchpad);
EXTRA_BIND(misc2); EXTRA_BIND(misc3); EXTRA_BIND(misc4); EXTRA_BIND(misc5); EXTRA_BIND(misc6);
EXTRA_BIND(touch1); EXTRA_BIND(touch2);
#undef EXTRA_BIND
REXCVAR_DEFINE_BOOL(pdz_gyro_invert_x,false,"Input/Gyro","Invert horizontal gyro");
REXCVAR_DEFINE_BOOL(pdz_gyro_invert_y,false,"Input/Gyro","Invert vertical gyro");
#define MENU_BIND(n,v) REXCVAR_DEFINE_STRING(pdz_menu_##n,v,"Input/Menu","Game menu navigation")
MENU_BIND(up,"Up"); MENU_BIND(down,"Down"); MENU_BIND(left,"Left"); MENU_BIND(right,"Right");
MENU_BIND(confirm,"Return"); MENU_BIND(cancel,"Backspace"); MENU_BIND(previous,"Q"); MENU_BIND(next,"E");
MENU_BIND(start,"Escape"); MENU_BIND(back,"Tab");
#undef MENU_BIND
#define PAD_BIND(n,v) REXCVAR_DEFINE_INT32(pdz_pad_##n,v,"Input","Controller input destination").range(-1,15)
PAD_BIND(up,0); PAD_BIND(down,1); PAD_BIND(left,2); PAD_BIND(right,3);
PAD_BIND(start,4); PAD_BIND(back,5); PAD_BIND(ls,6); PAD_BIND(rs,7);
PAD_BIND(lb,8); PAD_BIND(rb,9); PAD_BIND(a,10); PAD_BIND(b,11);
PAD_BIND(x,12); PAD_BIND(y,13); PAD_BIND(lt,14); PAD_BIND(rt,15);
#undef PAD_BIND
#define MENU_PAD(n,v) REXCVAR_DEFINE_INT32(pdz_menu_pad_##n,v,"Input/Menu","Controller menu destination").range(-1,15)
MENU_PAD(up,0); MENU_PAD(down,1); MENU_PAD(left,2); MENU_PAD(right,3);
MENU_PAD(start,4); MENU_PAD(back,5); MENU_PAD(ls,6); MENU_PAD(rs,7);
MENU_PAD(lb,8); MENU_PAD(rb,9); MENU_PAD(a,10); MENU_PAD(b,11);
MENU_PAD(x,12); MENU_PAD(y,13); MENU_PAD(lt,14); MENU_PAD(rt,15);
#undef MENU_PAD
// Game-side input policy. The SDK supplies physical provenance/sensor samples;
// this layer owns bindings, activation, calibration and camera-turn conversion.
// The device filter can run on guest polling or UI polling while the overlay is
// open. pad_mutex protects the shared pad/sensor snapshots; SDK calls that take
// its state lock belong outside that mutex. Do not add a reverse lock ordering.
namespace {
using Clock=std::chrono::steady_clock;
struct Pad {pdz::input::Point raw;Clock::time_point time;};
struct MotionState {
 rex::input::MotionSample sample;
 std::vector<rex::input::MotionSample> pending;
 uint64_t processed_sample=0,rumble_until=0;
 rex::input::NativeInputCapabilities capabilities;
 Clock::time_point capabilities_time;
 std::array<double,3> bias{},sum{};
 bool available=false,calibrating=false;
 uint32_t samples=0;
 uint64_t last_sample=0,calibration_start=0;
 pdz::input::GyroSpace orientation;
 pdz::input::Point projected;
 uint64_t orientation_sample=0;
 int orientation_space=0;
 uint16_t buttons=0;
 uint64_t extra_buttons=0;
 Clock::time_point time;
 std::string name;
};
std::unordered_map<uint64_t,MotionState> motion_pads;
std::string gyro_status="Enable gyro, then return to gameplay to detect a sensor.";
bool calibrate_gyro=false;
std::mutex pad_mutex;
std::unordered_map<uint64_t,Pad> pads;
struct OverlayState {
 pdz::input::StickCameraShortcuts camera_shortcuts;
 pdz::ui::OverlayPad pad;pdz::ui::OverlayShortcuts shortcuts;Clock::time_point time;
 bool captured=false,drain=false;uint32_t packet=1;
 rex::input::X_INPUT_GAMEPAD previous_output{};
};
std::unordered_map<uint64_t,OverlayState> overlay_pads;
std::atomic<bool> overlay_capture{false};
std::atomic<unsigned> overlay_toggles{0},render_toggles{0};
std::atomic<unsigned> shoulder_toggles{0};
std::atomic<unsigned> perspective_toggles{0};
int listening=-1;
constexpr const char* pad_names[]={"D-pad up","D-pad down","D-pad left","D-pad right","Start","Back","Left stick click","Right stick click","LB","RB","A","B","X","Y","LT","RT"};
constexpr const char* extra_names[]={"Right rear 1 / R4","Left rear 1 / L4","Right rear 2 / R5","Left rear 2 / L5","Touchpad Click Left","Touchpad Click Right","Left Stick Touch","Right Stick Touch","Left Grip Touch","Right Grip Touch","Touchpad Touch Left","Touchpad Touch Right"};
constexpr const char* generic_extra_names[]={"Right rear 1","Left rear 1","Right rear 2","Left rear 2","Touchpad Click","Extra button 2","Extra button 3","Extra button 4","Extra button 5","Extra button 6","Touchpad 1 Contact","Touchpad 2 Contact"};
constexpr const char* extra_flags[]={"pdz_extra_r4","pdz_extra_l4","pdz_extra_r5","pdz_extra_l5","pdz_extra_touchpad","pdz_extra_misc2","pdz_extra_misc3","pdz_extra_misc4","pdz_extra_misc5","pdz_extra_misc6","pdz_extra_touch1","pdz_extra_touch2"};
constexpr const char* pad_flags[]={"pdz_pad_up","pdz_pad_down","pdz_pad_left","pdz_pad_right","pdz_pad_start","pdz_pad_back","pdz_pad_ls","pdz_pad_rs","pdz_pad_lb","pdz_pad_rb","pdz_pad_a","pdz_pad_b","pdz_pad_x","pdz_pad_y","pdz_pad_lt","pdz_pad_rt"};
constexpr const char* menu_pad_flags[]={"pdz_menu_pad_up","pdz_menu_pad_down","pdz_menu_pad_left","pdz_menu_pad_right","pdz_menu_pad_start","pdz_menu_pad_back","pdz_menu_pad_ls","pdz_menu_pad_rs","pdz_menu_pad_lb","pdz_menu_pad_rb","pdz_menu_pad_a","pdz_menu_pad_b","pdz_menu_pad_x","pdz_menu_pad_y","pdz_menu_pad_lt","pdz_menu_pad_rt"};
struct Bind {const char* flag;const char* label;const char* modern;const char* original;};
constexpr Bind binds[]={
 {"keybind_lstick_up","Move forward","W","W"},{"keybind_lstick_down","Move backward","S","S"},
 {"keybind_lstick_left","Move left","A","A"},{"keybind_lstick_right","Move right","D","D"},
 {"keybind_right_trigger","Fire / RT","LMB","E,O"},{"keybind_left_trigger","Aim / left-hand fire / LT","RMB","Q,I"},
 {"keybind_x","Reload / X","R","L"},{"keybind_y","Switch / hold to swap / Y","Q","P"},
 {"keybind_a","Action / A","Space","Semicolon,Space"},{"keybind_b","Roll / B","C","Quote,Backspace"},
 {"keybind_left_shoulder","Left shoulder","1","1"},{"keybind_right_shoulder","Right shoulder","3","3"},
 {"keybind_lstick_press","Left stick click","F","F"},{"keybind_rstick_press","Right stick click","V","K"},
 {"keybind_dpad_up","D-pad up","Up","Shift+Up"},{"keybind_dpad_down","D-pad down","Down","Shift+Down"},
 {"keybind_dpad_left","Gadget previous / zoom out","Left","Shift+Left"},{"keybind_dpad_right","Gadget next / zoom in","Right","Shift+Right"},
 {"keybind_start","Pause / Start","Return","X,Return"},{"keybind_back","Back / objectives","Tab","Z,Tab"},
 {"keybind_rstick_up","Keyboard look up","","Up"},{"keybind_rstick_down","Keyboard look down","","Down"},
 {"keybind_rstick_left","Keyboard look left","","Left"},{"keybind_rstick_right","Keyboard look right","","Right"},
 {"keybind_guide","Guide","",""},
 {"pdz_menu_up","Menu up","Up","Up"},{"pdz_menu_down","Menu down","Down","Down"},
 {"pdz_menu_left","Menu left","Left","Left"},{"pdz_menu_right","Menu right","Right","Right"},
 {"pdz_menu_confirm","Confirm / A","Return","Return"},{"pdz_menu_cancel","Cancel / B","Backspace","Backspace"},
 {"pdz_menu_previous","Previous tab / LB","Q","Q"},{"pdz_menu_next","Next tab / RB","E","E"},
 {"pdz_menu_start","Menu / resume / Start","Escape","Escape"},{"pdz_menu_back","Back / view","Tab","Tab"}};
constexpr int gameplay_bind_count=25;
std::string CaptureKey(ImGuiKey key) {
 if(key>=ImGuiKey_A&&key<=ImGuiKey_Z) return std::string(1,char('A'+key-ImGuiKey_A));
 if(key>=ImGuiKey_0&&key<=ImGuiKey_9) return std::string(1,char('0'+key-ImGuiKey_0));
 if(key>=ImGuiKey_F1&&key<=ImGuiKey_F12) return "F"+std::to_string(1+key-ImGuiKey_F1);
 switch(key) {
 case ImGuiKey_MouseLeft:return "LMB";case ImGuiKey_MouseRight:return "RMB";case ImGuiKey_MouseMiddle:return "MMB";
 case ImGuiKey_MouseX1:return rex::ui::VirtualKeyToString(rex::ui::VirtualKey::kXButton1);
 case ImGuiKey_MouseX2:return rex::ui::VirtualKeyToString(rex::ui::VirtualKey::kXButton2);
 case ImGuiKey_LeftShift:case ImGuiKey_RightShift:return "Shift";
 case ImGuiKey_LeftCtrl:case ImGuiKey_RightCtrl:return "Ctrl";
 case ImGuiKey_LeftAlt:case ImGuiKey_RightAlt:return "Alt";
 case ImGuiKey_Enter:return "Return";case ImGuiKey_Space:return "Space";case ImGuiKey_Tab:return "Tab";
 case ImGuiKey_UpArrow:return "Up";case ImGuiKey_DownArrow:return "Down";case ImGuiKey_LeftArrow:return "Left";case ImGuiKey_RightArrow:return "Right";
 case ImGuiKey_Backspace:return "Backspace";case ImGuiKey_Delete:return "Delete";
 default: {auto name=std::string(ImGui::GetKeyName(key));return rex::ui::ParseVirtualKey(name)!=rex::ui::VirtualKey::kNone?name:"";}
 }
}
}
bool PDZ_GameMenuActive() {
   if(pdz::UiMouseCaptured()) return false;
   // Locktopus sets the pause-like player flag while its simulation is active.
   // Only its recent, owner-checked native input consumer grants this exception.
   if(PDZ_LocktopusInputActive()) return false;
   auto* runtime=rex::Runtime::instance();auto* base=runtime?runtime->virtual_membase():nullptr;
   if(!base) return false;
   const auto readable=[base](uint32_t p,size_t n=4) {
     if(p<0x10000u || p>UINT32_MAX-n) return false;
     auto access=rex::memory::PageAccess::kNoAccess;
     return rex::memory::QueryProtect(base+p,n,access)&&access!=rex::memory::PageAccess::kNoAccess;
   };
   const auto word=[base](uint32_t p){return rex::memory::load_and_swap<uint32_t>(base+p);};
   if(!readable(0x82E35468u)) return false;
   const auto root=word(0x82E35468u);
   if(!root) return true; // Front end: no local player.
   if(root>UINT32_MAX-0x3BCu || !readable(root+0x3B8u)) return false;
   const auto player=word(root+0x3B8u);
   if(!player) return true;
   if(!readable(player,0x2000u)||word(player)!=0x820C93A0u) return false;
   // Special cameras also set the pause-like player flag. Keep gameplay binds there.
   const auto fov=readable(0x82D68320u)?word(0x82D68320u):0;
   const auto mode=fov&&fov<=UINT32_MAX-0x540u&&readable(fov+0x53Cu)?word(fov+0x53Cu):0;
   if(mode&&mode<=UINT32_MAX-12u&&readable(mode+11u,1)&&base[mode+11u]==4) return false;
   return base[player+0x16A3u]!=0;
}
void PDZ_InitializeInput() {
 auto* rt=rex::Runtime::instance();
 auto* system=rt?dynamic_cast<rex::input::InputSystem*>(rt->input_system()):nullptr;
 if(!system) return;
 rex::input::mnk::SetMenuNavigationCallback([] {
   return rex::cvar::Query<bool>("pdz_menu_bindings")&&PDZ_GameMenuActive();
 });
 REXLOG_INFO("PDZ input filter installed: physical player-1 pads only; keyboard/mouse unchanged");
 system->SetDeviceFilter([system](uint32_t user,const rex::input::DeviceInfo& info,rex::input::X_INPUT_STATE& state) {
   if(user!=0||info.synthetic) return;
   rex::input::NativeInputCapabilities capabilities;
   Clock::time_point capabilities_time;
   {std::lock_guard lock(pad_mutex);if(auto it=motion_pads.find(uint64_t(info.id));it!=motion_pads.end()){capabilities=it->second.capabilities;capabilities_time=it->second.capabilities_time;}}
   if(Clock::now()-capabilities_time>std::chrono::seconds(1)){
     capabilities=system->GetDeviceNativeCapabilities(info.id);capabilities_time=Clock::now();
   }
   rex::input::MotionSample motion;
   std::vector<rex::input::MotionSample> motion_samples;
   const bool motion_enabled=rex::cvar::Query<std::string>("pdz_gyro_mode")!="off";
   const bool available=motion_enabled&&system->GetDeviceMotion(info.id,motion);
   uint64_t processed=0;
   {std::lock_guard lock(pad_mutex);if(auto it=motion_pads.find(uint64_t(info.id));it!=motion_pads.end())processed=it->second.processed_sample;}
   if(available)system->GetDeviceMotionSamples(info.id,processed,motion_samples);
   const uint16_t physical_buttons=state.gamepad.buttons;
   const uint64_t extra_buttons=system->GetDeviceExtraButtons(info.id);
   const bool camera_shortcuts_allowed=
       !PDZ_GameMenuActive()&&!PDZ_LocktopusInputActive()&&!overlay_capture.load();
   {std::lock_guard lock(pad_mutex);
     const auto now=Clock::now();auto& overlay=overlay_pads[uint64_t(info.id)];
     const auto& gamepad=state.gamepad;
     overlay.pad={physical_buttons,int16_t(gamepad.thumb_lx)/32767.f,int16_t(gamepad.thumb_ly)/32767.f,
                  gamepad.left_trigger/255.f,gamepad.right_trigger/255.f,true,capabilities.quick_access};
     overlay.time=now;
     const bool captured=overlay_capture.load();
     if(overlay.captured&&!captured)overlay.drain=true;
     overlay.captured=captured;
     const auto shortcuts=overlay.shortcuts.Update(physical_buttons,capabilities.quick_access&&(extra_buttons&pdz::ui::kQuickAccess),
         std::chrono::duration<double>(now.time_since_epoch()).count(),captured);
     if(shortcuts.overlay)overlay_toggles.fetch_add(1);
     if(shortcuts.render)render_toggles.fetch_add(1);
     state.gamepad.buttons=shortcuts.buttons;
     const auto camera=overlay.camera_shortcuts.Update(uint16_t(state.gamepad.buttons),
         std::chrono::duration<double>(now.time_since_epoch()).count(),
         camera_shortcuts_allowed&&!overlay.drain,rex::cvar::Query<bool>("pdz_tp_enable"));
     state.gamepad.buttons=camera.buttons;
     if(camera.perspective)perspective_toggles.fetch_add(1);
     if(camera.shoulder)shoulder_toggles.fetch_add(1);
     if(overlay.drain){
       const bool neutral=!physical_buttons&&gamepad.left_trigger<25&&gamepad.right_trigger<25&&
           std::hypot(int16_t(gamepad.thumb_lx)/32767.,int16_t(gamepad.thumb_ly)/32767.)<.25&&
           std::hypot(int16_t(gamepad.thumb_rx)/32767.,int16_t(gamepad.thumb_ry)/32767.)<.25;
       if(neutral)overlay.drain=false;else state.gamepad={};
     }
   }
   const bool menu=PDZ_GameMenuActive();
   const bool separate=menu&&rex::cvar::Query<bool>("pdz_controller_menu_remap");
   if(separate||rex::cvar::Query<bool>("pdz_controller_remap")) {
     // Select directly from physical input; never cascade two remaps.
     const auto& flags=separate?menu_pad_flags:pad_flags;
     std::array<int,16> map;for(int i=0;i<16;++i) map[i]=rex::cvar::Query<int32_t>(flags[i]);
     auto& g=state.gamepad;
     const auto mapped=pdz::input::Remap({uint16_t(g.buttons),g.left_trigger,g.right_trigger},map);
     g.buttons=mapped.bits;g.left_trigger=mapped.lt;g.right_trigger=mapped.rt;
   }
   if(rex::cvar::Query<bool>("pdz_extra_buttons"))for(int i=0;i<int(std::size(extra_flags));++i)if(extra_buttons&(uint64_t(1)<<(16+i))){
     const int target=rex::cvar::Query<int32_t>(extra_flags[i]);
     if(target>=0&&target<14)state.gamepad.buttons=uint16_t(state.gamepad.buttons)|pdz::input::masks[target];
     else if(target==14)state.gamepad.left_trigger=255;
     else if(target==15)state.gamepad.right_trigger=255;
   }
   std::lock_guard lock(pad_mutex);
   // Deferred Menu/View taps and release guards can change output without a
   // new hardware packet. Keep the guest's change counter truthful.
   auto& overlay=overlay_pads[uint64_t(info.id)];const auto& out=state.gamepad;const auto& old=overlay.previous_output;
   if(out.buttons!=old.buttons||out.left_trigger!=old.left_trigger||out.right_trigger!=old.right_trigger||
      out.thumb_lx!=old.thumb_lx||out.thumb_ly!=old.thumb_ly||out.thumb_rx!=old.thumb_rx||out.thumb_ry!=old.thumb_ry)++overlay.packet;
   overlay.previous_output=out;state.packet_number=overlay.packet;
   pads[uint64_t(info.id)]={{int16_t(state.gamepad.thumb_rx)/32767.,int16_t(state.gamepad.thumb_ry)/32767.},Clock::now()};
   auto& sensor=motion_pads[uint64_t(info.id)];
   sensor.capabilities=capabilities;sensor.capabilities_time=capabilities_time;
   sensor.sample=motion;sensor.available=available;sensor.time=Clock::now();
   sensor.pending=std::move(motion_samples);
   sensor.name=info.name;sensor.buttons=physical_buttons;sensor.extra_buttons=extra_buttons;
   if(available&&calibrate_gyro){sensor.calibrating=true;sensor.sum={};sensor.samples=0;sensor.calibration_start=0;sensor.orientation.Reset();sensor.orientation_sample=0;calibrate_gyro=false;}
   if(sensor.calibrating&&motion.timestamp_ns&&motion.timestamp_ns!=sensor.last_sample&&
      std::isfinite(motion.pitch)&&std::isfinite(motion.yaw)&&std::isfinite(motion.roll)) {
     sensor.last_sample=motion.timestamp_ns;
     if(!sensor.calibration_start)sensor.calibration_start=motion.timestamp_ns;
     if(std::hypot(motion.pitch,std::hypot(motion.yaw,motion.roll))>.05) {
       sensor.sum={};sensor.samples=0;sensor.calibration_start=motion.timestamp_ns;
     } else {
       sensor.sum[0]+=motion.pitch;sensor.sum[1]+=motion.yaw;sensor.sum[2]+=motion.roll;++sensor.samples;
       if(sensor.samples>=30&&motion.timestamp_ns-sensor.calibration_start>=2000000000ULL){
         for(int i=0;i<3;++i)sensor.bias[i]=sensor.sum[i]/sensor.samples;
         sensor.calibrating=false;gyro_status="Calibration complete: "+sensor.name;
       }
     }
   }
 });
}
pdz::ui::OverlayPad PDZ_OverlayController() {
 std::lock_guard lock(pad_mutex);pdz::ui::OverlayPad result;uint64_t selected=UINT64_MAX;
 for(auto it=overlay_pads.begin();it!=overlay_pads.end();){
   if(Clock::now()-it->second.time>std::chrono::milliseconds(500)){it=overlay_pads.erase(it);continue;}
   const auto& pad=it->second.pad;
   // Prefer an actively operated physical pad, otherwise the first assigned one.
   const bool active=pad.buttons||std::hypot(pad.lx,pad.ly)>.25;
   const bool result_active=result.buttons||std::hypot(result.lx,result.ly)>.25;
   if(!result.connected||(active&&!result_active)||(active==result_active&&it->first<selected)){result=pad;selected=it->first;}
   ++it;
 }
 return result;
}
bool PDZ_ConsumeOverlayToggle(){return (overlay_toggles.exchange(0)&1)!=0;}
bool PDZ_ConsumeRenderToggle(){return (render_toggles.exchange(0)&1)!=0;}
bool PDZ_ConsumeShoulderToggle(){return (shoulder_toggles.exchange(0)&1)!=0;}
bool PDZ_ConsumePerspectiveToggle(){return (perspective_toggles.exchange(0)&1)!=0;}
void PDZ_SetOverlayControllerCapture(bool captured){overlay_capture.store(captured);}
bool PDZ_BindingCaptureActive(){return listening>=0;}
pdz::input::Point PDZ_GyroTurn(bool aiming) {
 // Integrate each monotonically stamped sensor sample once. UI/guest polls can
 // observe the same SDK history; processed_sample is the consumption watermark.
 // This path deliberately does not use the stick curve or stick sensitivity.
 static thread_local auto previous=Clock::time_point{};
 static thread_local uint64_t device=0;
 static thread_local pdz::input::GyroFilter filter;
 const auto now=Clock::now();
 const double dt=previous==Clock::time_point{}?0:std::chrono::duration<double>(now-previous).count();previous=now;
 const auto mode=rex::cvar::Query<std::string>("pdz_gyro_mode");
 std::lock_guard lock(pad_mutex);
 MotionState* selected=nullptr;uint64_t id=0;
 for(auto it=motion_pads.begin();it!=motion_pads.end();) {
   if(now-it->second.time>std::chrono::milliseconds(100)){it=motion_pads.erase(it);continue;}
   if(it->second.available&&(!selected||it->first<id)){selected=&it->second;id=it->first;}
   ++it;
 }
 if(mode!="off")gyro_status=selected?(selected->calibrating?"Keep controller still for two seconds: ":"Native gyro detected: ")+selected->name:"No motion sensor available to this input path. Steam Input may be translating or hiding the physical controller.";
 const int clutch=rex::cvar::Query<int32_t>("pdz_gyro_clutch");
 const int enable_input=rex::cvar::Query<int32_t>("pdz_gyro_enable_input");
 const bool unavailable_gate=selected&&enable_input>=16&&enable_input<64&&!(selected->capabilities.extra_buttons&(uint64_t(1)<<enable_input));
 const bool activation_held=selected&&(enable_input<0||unavailable_gate||(enable_input<14&&(selected->buttons&pdz::input::masks[enable_input]))||(enable_input>=16&&enable_input<64&&(selected->extra_buttons&(uint64_t(1)<<enable_input))));
 if(mode=="off"||!selected||!selected->sample.timestamp_ns||selected->calibrating||pdz::UiMouseCaptured()||PDZ_GameMenuActive()||
    (mode=="ads"&&!aiming)||(clutch>=0&&clutch<14&&(selected->buttons&pdz::input::masks[clutch]))||
    (clutch>=16&&clutch<=27&&(selected->extra_buttons&(uint64_t(1)<<clutch)))||!activation_held||dt<=0||dt>.1) {
   if(selected){selected->processed_sample=selected->sample.timestamp_ns;selected->pending.clear();selected->rumble_until=0;}
   filter.Reset();return {};
 }
 if(id!=device){device=id;filter.Reset();selected->processed_sample=selected->sample.timestamp_ns;selected->pending.clear();return {};}
 pdz::input::Point total;
 for(const auto& sample:selected->pending) {
 if(sample.timestamp_ns<=selected->processed_sample)continue;
 const double sample_dt=selected->processed_sample?double(sample.timestamp_ns-selected->processed_sample)*1e-9:0.;
 selected->processed_sample=sample.timestamp_ns;
 if(sample_dt<=0||sample_dt>.1){filter.Reset();selected->orientation.Reset();selected->orientation_sample=0;continue;}
 double pitch=sample.pitch-selected->bias[0],yaw=sample.yaw-selected->bias[1],roll=sample.roll-selected->bias[2];
 const auto space_name=rex::cvar::Query<std::string>("pdz_gyro_space");
 const int space=space_name=="player"?1:space_name=="world"?2:0;
 if(space!=selected->orientation_space){selected->orientation.Reset();selected->orientation_sample=0;selected->orientation_space=space;filter.Reset();}
 if(space){
   if(!sample.accel_timestamp_ns||std::abs(double(sample.timestamp_ns)-double(sample.accel_timestamp_ns))>100000000.){
     selected->orientation.Reset();selected->orientation_sample=0;filter.Reset();gyro_status="Player/World space needs a fresh accelerometer sample: "+selected->name;continue;
   }
   if(sample.timestamp_ns!=selected->orientation_sample){
     const double sensor_dt=selected->orientation_sample&&sample.timestamp_ns>selected->orientation_sample?double(sample.timestamp_ns-selected->orientation_sample)*1e-9:sample_dt;
     selected->projected=selected->orientation.Project(pitch,yaw,roll,sample.accel_x,sample.accel_y,sample.accel_z,sensor_dt,space);
     selected->orientation_sample=sample.timestamp_ns;
   }
   yaw=selected->projected.x;pitch=selected->projected.y;roll=0;
 }
 if(sample.rumble_strength>0&&sample.timestamp_ns>=sample.rumble_timestamp_ns&&
    sample.timestamp_ns-sample.rumble_timestamp_ns<65535000000ULL)
   selected->rumble_until=sample.timestamp_ns+80000000ULL;
 const bool rumble_filter=rex::cvar::Query<bool>("pdz_gyro_rumble_filter")&&sample.timestamp_ns<selected->rumble_until;
 const bool tiered=rumble_filter||rex::cvar::Query<bool>("pdz_gyro_tiered_smoothing");
 const double smooth_speed=rex::cvar::Query<double>("pdz_gyro_smooth_speed");
 const double normal_smoothing=rex::cvar::Query<double>("pdz_gyro_smoothing");
 // Only motor activity strengthens smoothing; zero remains a full opt-out.
 const double effective_smoothing=rumble_filter?normal_smoothing*1.5:normal_smoothing;
 const auto turn=filter.Update(pitch,yaw,roll,sample_dt,
   rex::cvar::Query<double>("pdz_gyro_sensitivity")*(aiming?rex::cvar::Query<double>("pdz_gyro_ads"):1.),
   rex::cvar::Query<double>("pdz_gyro_pitch_scale"),rex::cvar::Query<double>("pdz_gyro_yaw_scale"),
   rex::cvar::Query<double>("pdz_gyro_threshold"),effective_smoothing,
   space?0:rex::cvar::Query<int32_t>("pdz_gyro_axis"),rex::cvar::Query<bool>("pdz_gyro_invert_x"),rex::cvar::Query<bool>("pdz_gyro_invert_y"),rex::cvar::Query<std::string>("pdz_gyro_acceleration"),
   tiered,rumble_filter?std::max(smooth_speed,50.):smooth_speed);
 total.x+=turn.x;total.y+=turn.y;
 }
 selected->pending.clear();return total;
}
pdz::input::Point PDZ_ControllerTurn(bool aiming) {
 static thread_local auto previous=Clock::time_point{};
 static thread_local pdz::input::Response response;
 const auto now=Clock::now();
 const double dt=previous==Clock::time_point{}?0:std::chrono::duration<double>(now-previous).count();previous=now;
 const bool use_modern=rex::cvar::Query<std::string>("pdz_stick_curve")!="original"||
     rex::cvar::Query<std::string>("pdz_gyro_mode")!="off";
 if(pdz::UiMouseCaptured()||!use_modern||dt<0||dt>.1){response.Reset();return {};}
 pdz::input::Point raw;
 {std::lock_guard lock(pad_mutex);for(auto it=pads.begin();it!=pads.end();) {
   if(now-it->second.time>std::chrono::milliseconds(100)){it=pads.erase(it);continue;}
   if(std::hypot(it->second.raw.x,it->second.raw.y)>std::hypot(raw.x,raw.y)) raw=it->second.raw;
   ++it;
 }}
 const auto target=pdz::input::Curve(raw,rex::cvar::Query<double>("pdz_stick_inner"),rex::cvar::Query<double>("pdz_stick_outer"),pdz::input::StickExponent(rex::cvar::Query<std::string>("pdz_stick_curve")));
 const auto curve=response.Update(target,dt,rex::cvar::Query<double>("pdz_stick_smoothing"),rex::cvar::Query<double>("pdz_stick_acceleration"));
 const double scale=dt*(aiming?rex::cvar::Query<double>("pdz_stick_ads"):1.);
 return {curve.x*rex::cvar::Query<double>("pdz_stick_yaw")*scale,
         -curve.y*rex::cvar::Query<double>("pdz_stick_pitch")*scale*(rex::cvar::Query<bool>("pdz_stick_invert_y")?-1.:1.)};
}
void PDZ_SaveInputSettings(toml::table& values) {
 values.insert_or_assign("vibration",rex::cvar::Query<bool>("vibration"));
 values.insert_or_assign("vibration_intensity",rex::cvar::Query<int32_t>("vibration_intensity"));
 for(auto flag:{"pdz_gyro_tiered_smoothing","pdz_gyro_rumble_filter"})values.insert_or_assign(flag,rex::cvar::Query<bool>(flag));
 values.insert_or_assign("pdz_gyro_smooth_speed",rex::cvar::Query<double>("pdz_gyro_smooth_speed"));
 for(auto flag:{"pdz_gyro_mode","pdz_gyro_space","pdz_gyro_acceleration"})values.insert_or_assign(flag,rex::cvar::Query<std::string>(flag));
 for(auto flag:{"pdz_gyro_steam_driver","pdz_gyro_invert_x","pdz_gyro_invert_y","pdz_extra_buttons"})values.insert_or_assign(flag,rex::cvar::Query<bool>(flag));
 for(auto flag:extra_flags)values.insert_or_assign(flag,rex::cvar::Query<int32_t>(flag));
 for(auto flag:{"pdz_gyro_axis","pdz_gyro_clutch","pdz_gyro_enable_input"})values.insert_or_assign(flag,rex::cvar::Query<int32_t>(flag));
 for(auto flag:{"pdz_gyro_sensitivity","pdz_gyro_ads","pdz_gyro_yaw_scale","pdz_gyro_pitch_scale","pdz_gyro_smoothing","pdz_gyro_threshold"})values.insert_or_assign(flag,rex::cvar::Query<double>(flag));
 for(auto& b:binds) values.insert_or_assign(b.flag,rex::cvar::Query<std::string>(b.flag));
 for(auto flag:pad_flags) values.insert_or_assign(flag,rex::cvar::Query<int32_t>(flag));
 for(auto flag:menu_pad_flags) values.insert_or_assign(flag,rex::cvar::Query<int32_t>(flag));
 values.insert_or_assign("pdz_controller_menu_remap",rex::cvar::Query<bool>("pdz_controller_menu_remap"));
 values.insert_or_assign("pdz_stick_curve",rex::cvar::Query<std::string>("pdz_stick_curve"));
 for(auto flag:{"pdz_stick_inner","pdz_stick_outer","pdz_stick_yaw","pdz_stick_pitch","pdz_stick_ads","pdz_stick_smoothing","pdz_stick_acceleration"}) values.insert_or_assign(flag,rex::cvar::Query<double>(flag));
 for(auto flag:{"pdz_stick_invert_y","pdz_controller_remap","pdz_wheel_zoom","pdz_menu_bindings"}) values.insert_or_assign(flag,rex::cvar::Query<bool>(flag));
}
void PDZ_DefaultPresetBindings(toml::table& values) {
 for(const auto& b:binds) values.insert_or_assign(b.flag,b.modern);
 for(int i=0;i<16;++i) {
   values.insert_or_assign(pad_flags[i],i);
   values.insert_or_assign(menu_pad_flags[i],i);
 }
 values.insert_or_assign("pdz_controller_remap",false);
 values.insert_or_assign("pdz_controller_menu_remap",false);
 values.insert_or_assign("pdz_menu_bindings",true);
}
void PDZ_DrawInputTab(bool& changed,PDZ_InputPage page) {
 const auto set=[&](const char* name,std::string value){if(rex::cvar::SetFlagByName(name,value))changed=true;};
 static int armed_frame=0;
 if(listening>=0&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false))listening=-1;
 static auto previous_page=page;
 if(previous_page!=page){listening=-1;previous_page=page;}
 bool connected=false,gyro_detected=false;
 uint64_t visible_extras=0,steam_extras=0;
 std::string detected_gyro_name;
 {std::lock_guard lock(pad_mutex);for(const auto& [id,sensor]:motion_pads){
   if(Clock::now()-sensor.time>std::chrono::milliseconds(500))continue;
   connected=true;visible_extras|=sensor.capabilities.extra_buttons;
   if(sensor.capabilities.steam_controller)steam_extras|=sensor.capabilities.extra_buttons;
   if(sensor.capabilities.gyro&&!gyro_detected){gyro_detected=true;detected_gyro_name=sensor.name;}
 }}
 const auto extra_label=[&](int i){return steam_extras&(uint64_t(1)<<(16+i))?extra_names[i]:generic_extra_names[i];};
 if(page==PDZ_InputPage::Gyro) {
   ImGui::SeparatorText("Native gyro");
   if(gyro_detected)ImGui::TextWrapped("Gyro recognised: %s",detected_gyro_name.c_str());
   else {
     ImGui::TextWrapped(connected?"Connected controller is not exposing a gyro to the game.":"No gyro controller recognised yet.");
     ImGui::TextWrapped("Connect a motion-capable controller. For Steam Controller, try enabling the native driver below and restart. If Steam is translating/hiding it, fully exit Steam for native testing. Standard Xbox controllers have no built-in gyro; a virtual Xbox pad cannot supply motion data.");
   }
   const auto mode=rex::cvar::Query<std::string>("pdz_gyro_mode");
   if(ImGui::BeginCombo("Gyro activation",mode.c_str())) {
     for(auto v:{"off","ads","always"})if(ImGui::Selectable(v,mode==v))set("pdz_gyro_mode",v);
     ImGui::EndCombo();
   }
   const auto space=rex::cvar::Query<std::string>("pdz_gyro_space");
   const char* space_label=space=="player"?"Player":space=="world"?"World":"Local";
   if(ImGui::BeginCombo("Gyro space",space_label)){
     for(auto v:{"local","player","world"})if(ImGui::Selectable(v==std::string_view("player")?"Player":v==std::string_view("world")?"World":"Local",space==v))set("pdz_gyro_space",v);
     ImGui::EndCombo();
   }
   ImGui::TextWrapped("Local: fixed controller axes. Player: adaptive yaw with local pitch. World: gravity-relative yaw and pitch. Player/World require an accelerometer.");
   const char* axes[]={"Yaw","Roll","Yaw + Roll"};
   ImGui::BeginDisabled(space!="local");
   int axis=rex::cvar::Query<int32_t>("pdz_gyro_axis");
   if(ImGui::Combo("Local horizontal axis",&axis,axes,3))set("pdz_gyro_axis",std::to_string(axis));
   ImGui::EndDisabled();
   ImGui::TextDisabled("Axis selection applies to Local space. Player / World choose axes from orientation.");
   bool steam=rex::cvar::Query<bool>("pdz_gyro_steam_driver");
   if(ImGui::Checkbox("Native Steam Controller driver (restart required)",&steam))set("pdz_gyro_steam_driver",steam?"true":"false");
   ImGui::TextWrapped("Native mode attempts direct controller access even when Steam is running, and ignores Steam virtual pads when a physical Steam gyro controller is available. Disable this option to use Steam Input compatibility instead. Steam desktop keyboard/mouse mappings may still need to be disabled in Steam.");
   ImGui::TextWrapped("Uses physical motion sensors, not mouse emulation. Disable Steam gyro-to-mouse when testing native gyro to avoid double input. If detection fails, Steam may still be claiming the physical device; try with Steam fully exited. A virtual Xbox pad cannot supply gyro.");
   {std::lock_guard lock(pad_mutex);ImGui::TextWrapped("%s",gyro_status.c_str());}
   ImGui::BeginDisabled(!gyro_detected||mode=="off");
   if(ImGui::Button("Calibrate resting gyro")){std::lock_guard lock(pad_mutex);calibrate_gyro=true;gyro_status="Calibration queued. Close F8 and keep controller still for two seconds.";}
   ImGui::EndDisabled();
   ImGui::TextWrapped("Calibration runs in gameplay; movement restarts the two-second measurement. Recalibrate after reconnecting. Bias is per device and per session.");
   const auto slider=[&](const char* label,const char* flag,float low,float high,const char* format){float v=float(rex::cvar::Query<double>(flag));if(pdz::ui::SliderFloat(label,&v,low,high,format))set(flag,std::to_string(v));};
   slider("Gyro sensitivity","pdz_gyro_sensitivity",.1,10,"%.2fx");
   const auto acceleration=rex::cvar::Query<std::string>("pdz_gyro_acceleration");
   const char* acceleration_label=acceleration=="linear"?"Linear":acceleration=="relaxed"?"Relaxed":acceleration=="aggressive"?"Aggressive":"None";
   if(ImGui::BeginCombo("Gyro acceleration",acceleration_label)) {
     for(auto v:{"none","linear","relaxed","aggressive"}) {
       const char* label=v==std::string_view("linear")?"Linear":v==std::string_view("relaxed")?"Relaxed":v==std::string_view("aggressive")?"Aggressive":"None";
       if(ImGui::Selectable(label,acceleration==v))set("pdz_gyro_acceleration",v);
     }
     ImGui::EndCombo();
   }
   ImGui::TextWrapped("None: constant sensitivity. Acceleration adds up to 2x sensitivity between 30 and 180 degrees/second. Relaxed delays the boost; Aggressive brings it in sooner. Linear is a straight acceleration ramp, not acceleration off. Independent of right-stick response.");
   slider("Gyro ADS multiplier","pdz_gyro_ads",.05,2,"%.2fx");
   slider("Gyro horizontal scale","pdz_gyro_yaw_scale",.1,3,"%.2fx");
   slider("Gyro vertical scale","pdz_gyro_pitch_scale",.1,3,"%.2fx");
   slider("Gyro smoothing","pdz_gyro_smoothing",0,.15,"%.3f s");
   for(auto flag:{"pdz_gyro_tiered_smoothing","pdz_gyro_rumble_filter"}) {
     bool enabled=rex::cvar::Query<bool>(flag);
     if(ImGui::Checkbox(flag==std::string_view("pdz_gyro_tiered_smoothing")?"Adaptive gyro smoothing":"Filter rumble vibration (experimental)",&enabled))set(flag,enabled?"true":"false");
   }
   slider("Smoothing bypass speed","pdz_gyro_smooth_speed",1,100,"%.1f deg/s");
   ImGui::TextWrapped("Adaptive smoothing steadies slow motion and bypasses smoothing for faster turns. Rumble filtering temporarily extends that range while motors run and briefly afterwards. Smoothing 0 disables both filters. Rumble filtering is experimental: it may also smooth deliberate slow aiming; hardware testing is required.");
   slider("Resting noise threshold","pdz_gyro_threshold",0,5,"%.2f deg/s");
   for(auto flag:{"pdz_gyro_invert_x","pdz_gyro_invert_y"}){bool v=rex::cvar::Query<bool>(flag);if(ImGui::Checkbox(flag==std::string_view("pdz_gyro_invert_x")?"Invert gyro X":"Invert gyro Y",&v))set(flag,v?"true":"false");}
   int clutch=rex::cvar::Query<int32_t>("pdz_gyro_clutch");
   const bool missing_clutch=clutch>=16&&clutch<64&&!(visible_extras&(uint64_t(1)<<clutch));
   if(ImGui::BeginCombo("Hold to pause gyro",missing_clutch?"Unavailable on this controller (ignored)":clutch<0||clutch==14||clutch==15?"None":clutch<14?pad_names[clutch]:extra_label(clutch-16))) {
     if(ImGui::Selectable("None",clutch<0))set("pdz_gyro_clutch","-1");
     for(int i=0;i<14;++i)if(ImGui::Selectable(pad_names[i],clutch==i))set("pdz_gyro_clutch",std::to_string(i));
     for(int i=0;i<int(std::size(extra_names));++i)if(visible_extras&(uint64_t(1)<<(16+i)))if(ImGui::Selectable(extra_label(i),clutch==16+i))set("pdz_gyro_clutch",std::to_string(16+i));
     ImGui::EndCombo();
   }
   ImGui::TextWrapped("Gyro follows Classic free aim when enabled, otherwise it turns the camera. The right stick turns the camera independently. Menus, focus loss, stale samples and calibration suppress gyro aim.");
   const int enable=rex::cvar::Query<int32_t>("pdz_gyro_enable_input");
   const bool missing_gate=enable>=16&&enable<64&&!(visible_extras&(uint64_t(1)<<enable));
   if(ImGui::BeginCombo("Hold / touch to enable gyro",missing_gate?"Unavailable on this controller (ignored)":enable<0||enable==14||enable==15?"No additional gate":enable<14?pad_names[enable]:extra_label(enable-16))){
     if(ImGui::Selectable("No additional gate",enable<0))set("pdz_gyro_enable_input","-1");
     for(int i=0;i<14;++i)if(ImGui::Selectable(pad_names[i],enable==i))set("pdz_gyro_enable_input",std::to_string(i));
     for(int i=0;i<int(std::size(extra_names));++i)if(visible_extras&(uint64_t(1)<<(16+i)))if(ImGui::Selectable(extra_label(i),enable==16+i))set("pdz_gyro_enable_input",std::to_string(16+i));
     ImGui::EndCombo();
   }
   ImGui::TextWrapped("Gyro and right stick are tuned independently. Stick response curves and stick sensitivity do not affect gyro. Gyro uses its own sensitivity, smoothing and noise threshold. Only Original falls back to Linear stick response while native gyro is enabled; Relaxed, Wide and Extra Wide remain selected.");
 }
 const auto draw_binds=[&](int first,int last) {
   for(int i=first;i<last;++i) {
     ImGui::PushID(i);const auto value=rex::cvar::Query<std::string>(binds[i].flag);
     ImGui::TextUnformatted(binds[i].label);ImGui::SameLine(ImGui::GetWindowContentRegionMax().x*.46f);
     if(ImGui::Button(listening==i?"Press input...":value.empty()?"Unbound":value.c_str())){listening=i;armed_frame=ImGui::GetFrameCount();}
     ImGui::SameLine();if(ImGui::SmallButton("Clear")){set(binds[i].flag,"");if(listening==i)listening=-1;}
     if(!value.empty())for(int j=first;j<i;++j)if(value==rex::cvar::Query<std::string>(binds[j].flag)) {ImGui::TextColored({1,.7f,.2f,1},"Shared with %s",binds[j].label);break;}
     ImGui::PopID();
   }
 };
 if(page==PDZ_InputPage::KeyboardMouse&&ImGui::CollapsingHeader("Keyboard & mouse",ImGuiTreeNodeFlags_DefaultOpen)) {
   if(ImGui::Button("Modern keyboard defaults"))for(int i=0;i<gameplay_bind_count;++i)set(binds[i].flag,binds[i].modern);
   ImGui::SameLine();if(ImGui::Button("Original keyboard defaults"))for(int i=0;i<gameplay_bind_count;++i)set(binds[i].flag,binds[i].original);
   ImGui::TextWrapped("Click a binding, then press a key or mouse button. Escape cancels; Clear unbinds. Changes apply live and save automatically.");
   draw_binds(0,gameplay_bind_count);
   bool wheel=rex::cvar::Query<bool>("pdz_wheel_zoom");if(ImGui::Checkbox("Mouse wheel zoom while aiming",&wheel))set("pdz_wheel_zoom",wheel?"true":"false");
   ImGui::TextWrapped("Locktopus: move the mouse to position the analog probe, then hold the mouse still to maintain it. Move back toward the centre to reduce its distance. Controller stick input takes over when moved.");
 }
 if(page==PDZ_InputPage::Menus&&ImGui::CollapsingHeader("Game menu keyboard navigation",ImGuiTreeNodeFlags_DefaultOpen)) {
   bool enabled=rex::cvar::Query<bool>("pdz_menu_bindings");
   if(ImGui::Checkbox("Separate game menu bindings",&enabled))set("pdz_menu_bindings",enabled?"true":"false");
   ImGui::TextWrapped("Used in the game's front end and pause menu, not the F8 overlay. Disabled preserves existing controls. Escape cancels binding capture; type Escape in the binding text field if wanted.");
   if(ImGui::Button("Restore menu defaults"))for(int i=gameplay_bind_count;i<int(std::size(binds));++i)set(binds[i].flag,binds[i].original);
   draw_binds(gameplay_bind_count,int(std::size(binds)));
   // Text entry permits keys reserved by the capture UI, and comma-separated alternatives.
   static char menu_start[128]{};
   if(!ImGui::IsAnyItemActive())std::snprintf(menu_start,sizeof(menu_start),"%s",rex::cvar::Query<std::string>("pdz_menu_start").c_str());
   if(ImGui::InputText("Menu / resume key text",menu_start,sizeof(menu_start),ImGuiInputTextFlags_EnterReturnsTrue))set("pdz_menu_start",menu_start);
 }
   if(listening>=0&&ImGui::GetFrameCount()>armed_frame) {
     if(ImGui::IsKeyPressed(ImGuiKey_Escape,false))listening=-1;
     else for(int k=ImGuiKey_NamedKey_BEGIN;k<ImGuiKey_NamedKey_END;++k) {
       if(!ImGui::IsKeyPressed(ImGuiKey(k),false))continue;
       if(k==ImGuiKey_F8||(k>=ImGuiKey_GamepadStart&&k<=ImGuiKey_GamepadRStickDown))continue;
       if(k>=ImGuiKey_LeftCtrl&&k<=ImGuiKey_RightSuper)continue; // Chords first; bare modifiers bind on release.
       auto key=CaptureKey(ImGuiKey(k));if(key.empty())continue;
       auto& io=ImGui::GetIO();if(io.KeyAlt)key="Alt+"+key;if(io.KeyCtrl)key="Ctrl+"+key;if(io.KeyShift)key="Shift+"+key;
       set(binds[listening].flag,key);listening=-1;break;
     }
     if(listening>=0)for(auto modifier:{ImGuiKey_LeftShift,ImGuiKey_RightShift,ImGuiKey_LeftCtrl,ImGuiKey_RightCtrl,ImGuiKey_LeftAlt,ImGuiKey_RightAlt})
       if(ImGui::IsKeyReleased(modifier)){set(binds[listening].flag,CaptureKey(modifier));listening=-1;break;}
   }
 if(page==PDZ_InputPage::Controller) {
   auto* runtime=rex::Runtime::instance();
   auto* system=runtime?dynamic_cast<rex::input::InputSystem*>(runtime->input_system()):nullptr;
   bool rumble=rex::cvar::Query<bool>("vibration");
   ImGui::BeginDisabled(!system);
   if(ImGui::Checkbox("Enable controller rumble",&rumble)&&system) {
     system->ToggleVibration(); // Also sends zero motor speeds immediately.
     changed=true;
   }
   int intensity=rex::cvar::Query<int32_t>("vibration_intensity");
   if(ImGui::SliderInt("Rumble intensity",&intensity,0,100,"%d%%")) {
     set("vibration_intensity",std::to_string(intensity));
     // Stop an already-running request; subsequent game requests use the new scale.
     rex::input::X_INPUT_VIBRATION silence{};
     for(uint32_t user=0;user<4;++user)system->SetState(user,&silence);
   }
   ImGui::EndDisabled();
 }
 if(page==PDZ_InputPage::Controller&&ImGui::CollapsingHeader("Controller look",ImGuiTreeNodeFlags_DefaultOpen)) {
   bool reload_action=rex::cvar::Query<bool>("pdz_reload_interact");
   if(ImGui::Checkbox("X: reload first, otherwise interact",&reload_action))set("pdz_reload_interact",reload_action?"true":"false");
   ImGui::TextWrapped("Hold A for cover. X sends a tap action only when the equipped weapon cannot reload; dual-wield and unrecognized weapon states keep native reload.");
   ImGui::TextWrapped("Native gyro has its own Gyro tab. The right stick turns the camera independently of mouse/gyro free aim.");
   const auto current=rex::cvar::Query<std::string>("pdz_stick_curve");
   const auto curve_label=[](std::string_view mode){return mode=="extra_wide"?"Extra Wide":mode=="wide"?"Wide":mode=="relaxed"?"Relaxed":mode=="linear"?"Linear":"Original";};
   if(ImGui::BeginCombo("Response",curve_label(current))){for(auto mode:{"original","linear","relaxed","wide","extra_wide"})if(ImGui::Selectable(curve_label(mode),current==mode))set("pdz_stick_curve",mode);ImGui::EndCombo();}
   ImGui::TextWrapped("Linear: direct stick response. Relaxed, Wide, Extra Wide: progressively wider fine-control ranges near centre, with the same maximum speed. These curves affect the right stick only, never gyro. Original preserves native controller behavior unless native gyro requires the modern path.");
   const auto slider=[&](const char* label,const char* flag,float low,float high,const char* format){float v=float(rex::cvar::Query<double>(flag));if(pdz::ui::SliderFloat(label,&v,low,high,format))set(flag,std::to_string(v));};
   slider("Inner deadzone","pdz_stick_inner",0,.4,"%.3f");slider("Outer threshold","pdz_stick_outer",.5,1,"%.3f");
   slider("Horizontal speed","pdz_stick_yaw",30,720,"%.0f deg/s");slider("Vertical speed","pdz_stick_pitch",30,720,"%.0f deg/s");slider("ADS sensitivity","pdz_stick_ads",.05,2,"%.2fx");
   slider("Optional smoothing","pdz_stick_smoothing",0,.15,"%.3f s");slider("Optional full-tilt acceleration","pdz_stick_acceleration",0,1,"%.2f");
   ImGui::TextDisabled("Both 0 = no added smoothing or acceleration.");
   bool invert=rex::cvar::Query<bool>("pdz_stick_invert_y");if(ImGui::Checkbox("Invert controller Y",&invert))set("pdz_stick_invert_y",invert?"true":"false");
   float samples[101];for(int i=0;i<=100;++i)samples[i]=float(pdz::input::Curve({i/100.,0},rex::cvar::Query<double>("pdz_stick_inner"),rex::cvar::Query<double>("pdz_stick_outer"),pdz::input::StickExponent(current)).x);
   ImGui::PlotLines("##stick_response",samples,101,0,"Modern curve preview: travel -> turn strength",0,1,{0,90});
   ImGui::TextDisabled("Original ignores these modern-look settings. Game input is blocked while this panel is open.");
 }
 if(page==PDZ_InputPage::Controller&&ImGui::CollapsingHeader("Controller buttons",ImGuiTreeNodeFlags_DefaultOpen)) {
   bool enabled=rex::cvar::Query<bool>("pdz_controller_remap");if(ImGui::Checkbox("Enable controller remapping",&enabled))set("pdz_controller_remap",enabled?"true":"false");
   ImGui::TextWrapped("Physical input -> game button. Player 1 pads only; keyboard unchanged. Menus inherit these bindings unless separate controller menu bindings are enabled. Steam-mapped buttons count as physical input; avoid mapping twice.");
   if(ImGui::Button("Restore controller buttons"))for(int i=0;i<16;++i)set(pad_flags[i],std::to_string(i));
   for(int i=0;i<16;++i){int target=rex::cvar::Query<int32_t>(pad_flags[i]);ImGui::PushID(i);if(ImGui::BeginCombo(pad_names[i],target<0?"Disabled":pad_names[target])){if(ImGui::Selectable("Disabled",target<0))set(pad_flags[i],"-1");for(int j=0;j<16;++j)if(ImGui::Selectable(pad_names[j],target==j))set(pad_flags[i],std::to_string(j));ImGui::EndCombo();}ImGui::PopID();}
 }
 if(page==PDZ_InputPage::Controller&&visible_extras&&ImGui::CollapsingHeader(steam_extras?"Steam Controller / Deck extra controls":"Additional controller buttons",ImGuiTreeNodeFlags_DefaultOpen)){
   bool extras=rex::cvar::Query<bool>("pdz_extra_buttons");if(ImGui::Checkbox("Enable independent extended bindings",&extras))set("pdz_extra_buttons",extras?"true":"false");
   ImGui::TextWrapped("Only controls exposed by a recognised controller are shown. Touch/grip inputs can enable or pause gyro in the Gyro tab without enabling these bindings. Controller rumble can be disabled above; bespoke haptics and trackpad aiming are not implemented.");
   for(int i=0;i<int(std::size(extra_flags));++i)if(visible_extras&(uint64_t(1)<<(16+i))){const int target=rex::cvar::Query<int32_t>(extra_flags[i]);ImGui::PushID(extra_flags[i]);if(ImGui::BeginCombo(extra_label(i),target<0?"Disabled":pad_names[target])){if(ImGui::Selectable("Disabled",target<0))set(extra_flags[i],"-1");for(int j=0;j<16;++j)if(ImGui::Selectable(pad_names[j],target==j))set(extra_flags[i],std::to_string(j));ImGui::EndCombo();}ImGui::PopID();}
 }
 if(page==PDZ_InputPage::Menus&&ImGui::CollapsingHeader("Game menu controller navigation",ImGuiTreeNodeFlags_DefaultOpen)) {
   bool enabled=rex::cvar::Query<bool>("pdz_controller_menu_remap");
   if(ImGui::Checkbox("Separate controller menu bindings",&enabled))
     set("pdz_controller_menu_remap",enabled?"true":"false");
   ImGui::TextWrapped("Front end and pause menu only, not F8. Independent of gameplay buttons and keyboard menu bindings. Stick navigation remains native.");
   if(ImGui::Button("Restore controller menu defaults"))
     for(int i=0;i<16;++i)set(menu_pad_flags[i],std::to_string(i));
   ImGui::PushID("menu_pad");
   for(int i=0;i<16;++i) {
     int target=rex::cvar::Query<int32_t>(menu_pad_flags[i]);
     ImGui::PushID(i);
     if(ImGui::BeginCombo(pad_names[i],target<0?"Disabled":pad_names[target])) {
       if(ImGui::Selectable("Disabled",target<0))set(menu_pad_flags[i],"-1");
       for(int j=0;j<16;++j)if(ImGui::Selectable(pad_names[j],target==j))
         set(menu_pad_flags[i],std::to_string(j));
       ImGui::EndCombo();
     }
     ImGui::PopID();
   }
   ImGui::PopID();
 }
}
