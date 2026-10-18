#include "pdz_qol.h"
// Guest-facing integration layer; the arithmetic/state machines live in the
// pdz_* headers below so they can be tested without running proprietary code.
// Generated call-site adapters supply TU0 guest addresses and register values.
// Keep native call ordering, guest units and player ownership checks intact when
// extracting a feature; this file is not a second renderer or input driver.
// See docs/ARCHITECTURE.md for the cross-thread snapshots and feature boundaries.
#include <cstring>
#include <cstdlib>
#include <thread>
#include "pdz_input.h"
#include "pdz_cover_view_math.h"
#include "pdz_cover_camera_anchor.h"
#include "pdz_cover_target_ray.h"
#include "pdz_cover_reprojection.h"
#include "pdz_ui_mouse.h"
#include "pdz_movement_math.h"
#include "pdz_weapon_timing.h"
#include "pdz_camspy_mouse.h"
#include "pdz_npc_damage_math.h"
#include "pdz_dodge_roll_math.h"
#include "pdz_weapon_controls.h"
#include "pdz_unarmed_controls.h"
#include "pdz_hud_bridge.h"
#include "pdz_weapon_sway_math.h"
#include "pdz_classic_aim_math.h"
#include "pdz_aim_retention.h"
#include "pdz_aim_assist_policy.h"
#include "pdz_reload_interact.h"
#include "pdz_locktopus_mouse.h"
#include "pdz_door_autoclose.h"
#include "pdz_presets.h"
#include "pdz_classic_scope.h"
#include "pdz_weapon_accuracy.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <bit>
#include <cmath>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>

#include <rex/cvar.h>
REXCVAR_DEFINE_BOOL(pdz_native_aim_assist, false, "Input",
                    "Use the in-game hip-fire aim-assist setting instead of overlay policy");
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/hook.h>

REX_EXTERN(sub_82252798);
bool PDZ_CopyThirdPersonClearanceEye(uint32_t player,float eye[3]);

REXCVAR_DEFINE_BOOL(pdz_p9p_hide_attachments, false, "Gameplay/Viewmodel",
                   "Experimental: hide local first-person P9P scope, flashlight and bracket");

namespace {
// Despite the historical name, this mutex also protects the live sway input
// snapshot. It must remain present when developer diagnostics are compiled out.
std::mutex aim_input_trace_mutex;
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
struct AimInputTrace {
  uint32_t player=0;
  double gyro_x=0,gyro_y=0,stick_x=0,stick_y=0;
  std::chrono::steady_clock::time_point time{};
} aim_input_trace;
#endif
struct SwayInput { uint32_t player=0; double yaw=0,pitch=0;
  std::chrono::steady_clock::time_point time{}; } sway_input;
}


// This native lookup is a leaf: no guest stack writes or animation mutation.
REX_EXTERN(sub_821E4578);

bool PDZ_AllowTimedDoorClose(uint8_t* base, uint32_t door) {
  if(!rex::cvar::Query<bool>("pdz_disable_warehouse_door_autoclose")) return true;
  // Invalid/unrecognized objects always keep the native close operation.
  const auto readable = [base](uint32_t address, uint32_t bytes) {
    if (!base || address < 0x10000u || address > UINT32_MAX - bytes) return false;
    size_t length = bytes;
    rex::memory::PageAccess access = rex::memory::PageAccess::kNoAccess;
    return rex::memory::QueryProtect(base + address, length, access) &&
           length >= bytes && access != rex::memory::PageAccess::kNoAccess;
  };
  const auto word = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  if (!readable(door, 88) || word(door) != 0x820C3E00u) return true;
  const uint32_t definition = word(door + 84);
  if (!readable(definition, 212) || base[definition + 141] != 1) return true;
  const auto expected = pdz::doors::TimedDoorModel(
      {word(definition + 4), word(definition + 8), word(definition + 12)});
  if (expected.empty()) return true;
  // Package relative string pointers are relocated by the native asset loader.
  // Require the exact, null-terminated model name as well as position bits.
  const uint32_t model = word(definition + 188);
  if (!readable(model, uint32_t(expected.size() + 1))) return true;
  if (std::string_view(reinterpret_cast<const char*>(base + model), expected.size())
          != expected || base[model + expected.size()] != 0) return true;
  return false;
}

namespace {
std::atomic<uint32_t> weapon_input{0};
std::atomic<uint8_t> weapon_right_trigger{0};
std::atomic<int> zoom_wheel{0};
std::atomic<int64_t> zoom_wheel_time{0};
std::atomic<int64_t> classic_hud_render_time{0};
std::atomic<int64_t> ammo_input_activity_time{0};
std::atomic<int64_t> classic_health_render_time{0};
std::atomic<float> classic_health_alpha{0};
int64_t HudTime() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool HudPointer(uint32_t p, uint32_t bytes) {
  // Arithmetic bounds only, NOT proof that the guest pages are mapped. Use
  // GuestReadable for optional/reconstructed pointers; native call-site inputs
  // still need their documented ownership/lifetime assumptions.
  return p >= 0x10000u && p <= UINT32_MAX - bytes;
}
uint32_t HudWord(uint8_t* base, uint32_t p) {
  return rex::memory::load_and_swap<uint32_t>(base+p);
}
bool GuestReadable(uint8_t* base,uint32_t p,uint32_t bytes) {
  if(!base||!HudPointer(p,bytes)) return false;
  size_t length=bytes;
  auto access=rex::memory::PageAccess::kNoAccess;
  return rex::memory::QueryProtect(base+p,length,access)&&length>=bytes&&
      access!=rex::memory::PageAccess::kNoAccess;
}
uint32_t LocalPlayer(uint8_t* base) {
  if(!GuestReadable(base,0x82E35468u,4))return 0;
  const auto root=HudWord(base,0x82E35468u);
  if(!GuestReadable(base,root,0x3BC))return 0;
  const auto player=HudWord(base,root+0x3B8);
  return GuestReadable(base,player,0x2000)&&HudWord(base,player)==0x820C93A0u?player:0;
}
bool FirstPerson(uint8_t* base) {
  if(!GuestReadable(base,0x82D68320u,4))return false;
  const auto manager=HudWord(base,0x82D68320u);
  if(!GuestReadable(base,manager,0x540))return false;
  const auto mode=HudWord(base,manager+0x53C);
  return GuestReadable(base,mode,12)&&base[mode+11]==0;
}
enum class EquippedStatus { Unknown, Unarmed, Armed };
EquippedStatus EquipmentStatus(uint8_t* base,uint32_t player) {
  if(!GuestReadable(base,player,4084))return EquippedStatus::Unknown;
  // Native equipment getter8253ECE8 treats weapon definition kind0 as fists.
  // Gadgets, mixed hands, invalid owners and unknown types never count as unarmed.
  for(uint32_t offset:{4076u,4080u}) {
    const auto item=HudWord(base,player+offset);
    if(!item)continue;
    if(!GuestReadable(base,item,1100)||HudWord(base,item+92)!=player)
      return EquippedStatus::Unknown;
    const auto type=HudWord(base,item+8), definition=HudWord(base,item+860);
    if(!GuestReadable(base,type,4)||!(HudWord(base,type)&16)||
       !GuestReadable(base,definition,1148))return EquippedStatus::Unknown;
    if(HudWord(base,definition+1144)!=0)return EquippedStatus::Armed;
  }
  return EquippedStatus::Unarmed;
}
struct UnarmedAction { uint32_t item=0,model=0,clip=0,action=0; };
std::mutex unarmed_action_mutex;
UnarmedAction unarmed_actions[2];
uint32_t unarmed_action_owner=0;
std::mutex scope_mutex;
std::unordered_map<uint32_t,bool> scope_scenes;
std::unordered_map<uint32_t,float> scope_selected;
uint64_t scope_session=0;
uint32_t scope_weapon=0;
int64_t scope_visible=0;
PDZ_ClassicScopeState scope_state;
bool AimHeld(uint8_t* base,uint32_t player);
bool ScopeActive(uint8_t* base,uint32_t player) {
  // Assist policy follows ADS input, not camera magnification. Third-person
  // keep-camera zoom may show a scope without narrowing the world-camera FOV.
  // AimHeld retains the existing dual-wield/left-hand-fire exclusion.
  if(AimHeld(base,player)) return true;
  const float blend=rex::memory::load_and_swap<float>(base+player+6416);
  if(!std::isfinite(blend)||blend<=.001f) return false;
  const uint32_t manager=HudWord(base,0x82D68320u);
  if(HudPointer(manager,0x444)) {
    const float normal=rex::memory::load_and_swap<float>(base+0x820EC228u);
    const float current=rex::memory::load_and_swap<float>(base+manager+0x440);
    // The same live magnification used by mouse zoom compensation: independent
    // of Classic overlay, resource naming, and whether scope art rendered yet.
    if(std::isfinite(normal)&&std::isfinite(current)&&normal>0&&current>0&&current<normal*.99f)
      return true;
  }
  const uint32_t weapon=HudWord(base,player+4080)?HudWord(base,player+4080):HudWord(base,player+4076);
  std::lock_guard lock(scope_mutex);
  return scope_weapon==weapon && scope_visible && HudTime()-scope_visible<150 &&
         std::isfinite(blend)&&blend>.001f;
}
bool DualHeld(uint8_t* base,uint32_t player) {
  return base&&HudPointer(player,4084)&&
      pdz::DualWeapons(HudWord(base,player+4076),HudWord(base,player+4080));
}
bool AimHeld(uint8_t* base,uint32_t player) {
  // A second equipped object owns LT as left-hand fire, not ADS.
  if(!base||!HudPointer(player,8208)||DualHeld(base,player)) return false;
  return (weapon_input.load()>>16)>=30;
}
std::mutex free_aim_mutex;
pdz::aim::FreeAim free_aim_state;
bool free_aim_gyro_source=false;
uint32_t free_aim_player=0,free_aim_weapon=0;
uint8_t free_aim_camera_mode=0xFF;
int64_t free_aim_ms=0;
void ResetFreeAim() {
  std::lock_guard lock(free_aim_mutex);
  free_aim_state.Reset(); free_aim_player=free_aim_weapon=0; free_aim_ms=0;
  free_aim_gyro_source=false;
  free_aim_camera_mode=0xFF;
}
bool ReadFreeAim(uint32_t player,pdz::aim::Point& point) {
  if(!rex::cvar::Query<bool>("pdz_classic_free_aim")||pdz::UiMouseCaptured()) return false;
  std::lock_guard lock(free_aim_mutex);
  if(!free_aim_ms || HudTime()-free_aim_ms>120 || (player&&player!=free_aim_player)) return false;
  auto* runtime=rex::Runtime::instance();
  uint8_t* base=runtime?runtime->virtual_membase():nullptr;
  if(!pdz::aim::ManualSwayActive(rex::cvar::Query<bool>("pdz_sway_during_ads"),
                                AimHeld(base,free_aim_player))) return false;
  point=free_aim_state.point; return true;
}
bool ReadAimProjection(uint8_t* base,uint32_t camera,pdz::aim::Projection& out) {
  if(!base||!HudPointer(camera,104)) return false;
  const uint32_t display=HudWord(base,0x82D6B2BCu);
  if(!HudPointer(display,112)) return false;
  const auto f=[base](uint32_t p){return double(rex::memory::load_and_swap<float>(base+p));};
  const double fov=f(camera+68),w=f(camera+96),h=f(camera+100);
  const double dw=f(display+104),dh=f(display+108),scale=f(camera+72);
  if(!pdz::aim::ValidAimFov(fov)||
     !std::isfinite(w)||!std::isfinite(h)||!std::isfinite(dw)||!std::isfinite(dh)||
     !std::isfinite(scale)||w<=0||h<=0||dw<=0||dh<=0||scale<=0) return false;
  out={std::tan(fov*.5),(w/h)/(dw/dh)*scale}; return pdz::aim::Valid(out);
}
bool ReadClassicHud(uint8_t* base, pdz::hud::LiveState& state, bool include_ammo) {
  if (!base) return false;
  const uint32_t root=HudWord(base,0x82E35468u);
  if(!HudPointer(root,0x3BCu)) return false;
  const uint32_t player=HudWord(base,root+0x3B8u);
  // HUD presentation is not input eligibility. Special-camera sequences also
  // set +16A3 (see PDZ_GameMenuActive); rejecting it restores native artwork
  // and sizing during traversal/death. Keep the validated live player/view,
  // and let native HUD render callbacks determine presentation visibility.
  if(!HudPointer(player,0x2000u)||HudWord(base,player)!=0x820C93A0u) return false;
  const uint32_t controller=HudWord(base,player+6560u);
  const uint32_t view=HudWord(base,0x82E2E7C4u);
  if(!HudPointer(controller,984)||!HudPointer(view,400)||!HudWord(base,view)) return false;
  const float health=rex::memory::load_and_swap<float>(base+controller+968u);
  const float recovery_ceiling=rex::memory::load_and_swap<float>(base+controller+972u);
  const float armour=rex::memory::load_and_swap<float>(base+controller+976u);
  if(!std::isfinite(health)||!std::isfinite(recovery_ceiling)||!std::isfinite(armour)) return false;
  state={};
  // Native progress-bar range is [0,100]. +972 is reduced separately by damage:
  // it is the remaining recovery ceiling, not an invariant normalization maximum.
  state.health=std::clamp(health/100.f,0.f,1.f);
  state.recoverable_health=std::clamp(recovery_ceiling/100.f,state.health,1.f);
  // Native barArmour XuiProgressBar has range [0,100], independently of health.
  state.shield=std::clamp(armour/100.f,0.f,1.f);
  // Native element callbacks only need gameplay/health eligibility. Never walk
  // weapon definitions or query Windows page protection for every UI element.
  // The overlay requests the full ammo snapshot once per overlay draw instead.
  if(!include_ammo) return true;
  // Non-firearm items must not invalidate independently verified health/HUD.
  const auto read_ammo=[&]() -> bool {
  const uint32_t weapon=HudWord(base,player+4080u);
  state.weapon=weapon;
  const auto ammo_input_time=ammo_input_activity_time.load(std::memory_order_relaxed);
  state.ammo_activity=!pdz::UiMouseCaptured()&&ammo_input_time&&HudTime()-ammo_input_time<150;
  if(!weapon) return true;
  if(!HudPointer(weapon,1330)) return false;
  const uint32_t weapon_vtable=HudWord(base,weapon);
  if(weapon_vtable<0x82000000u||weapon_vtable>=0x82140000u) return false;
  const uint32_t definition=HudWord(base,weapon+860u);
  if(!HudPointer(definition,200)) return false;
  const auto half=[base](uint32_t p) {return uint32_t(rex::memory::load_and_swap<uint16_t>(base+p));};
  const auto reserve_limit=[base](uint32_t type) -> uint32_t {
    // sub_824E80B0 returns records at manager+48 -> +36, stride 6;
    // pickup/refill paths read the first byte as the ammo-type carry limit.
    const auto readable=[base](uint32_t p,size_t n) {
      if(!HudPointer(p,uint32_t(n)))return false;
      auto access=rex::memory::PageAccess::kNoAccess;
      return rex::memory::QueryProtect(base+p,n,access)&&access!=rex::memory::PageAccess::kNoAccess;
    };
    if(type>15||!readable(0x82D6B2E0u,4))return 0;
    const auto manager=HudWord(base,0x82D6B2E0u);
    if(!HudPointer(manager,40)||!readable(manager+36,4))return 0;
    const auto records=HudWord(base,manager+36);
    if(!HudPointer(records,96)||!readable(records+type*6,1))return 0;
    return base[records+type*6];
  };
  state.loaded=half(weapon+1316u);
  state.capacity=half(definition+154u);
  const uint32_t pool=HudWord(base,weapon+1100u);
  const uint32_t type=base[definition+199u];
  if(type>15u||state.capacity>1000u) return false;
  if(pool&&!HudPointer(pool,84)) return false;
  state.reserve=pool?half(pool+(26u+type)*2u):half(weapon+1318u);
  state.reserve_capacity=reserve_limit(type);
  const uint32_t definitions=HudWord(base,weapon+1248u);
  if(HudPointer(definitions,8)) {
    const uint32_t primary=HudWord(base,definitions);
    const uint32_t secondary=HudWord(base,definitions+4u);
    // Only report a mode when the active definition matches a real function.
    // Special abilities can replace +860 with a cloned definition: don't guess.
    state.function_known=definition==primary||(secondary&&definition==secondary);
    state.secondary_function=secondary&&definition==secondary;
    if(HudPointer(secondary,200)) {
      const uint32_t secondary_type=base[secondary+199u];
      state.secondary_capacity=half(secondary+154u);
      state.secondary_loaded=half(weapon+1320u);
      if(secondary_type>15u||state.secondary_capacity>1000u) return false;
      state.secondary_reserve=pool?half(pool+(26u+secondary_type)*2u):half(weapon+1322u);
      state.secondary_reserve_capacity=reserve_limit(secondary_type);
    }
  }
  return true;
  };
  if(!read_ammo()) {
    state.loaded=state.capacity=state.reserve=0;
    state.secondary_loaded=state.secondary_capacity=state.secondary_reserve=0;
  }
  return true;
}
}

PDZ_NativeHudScaleScope::PDZ_NativeHudScaleScope(uint8_t* memory,uint32_t element) {
  if(!memory||!HudPointer(element,240)||!rex::cvar::Query<bool>("pdz_half_native_hud")) return;
  pdz::hud::LiveState live;
  if(!ReadClassicHud(memory,live,false)) return;
  const auto name=[memory](uint32_t p) {
    std::string result;
    if(!HudPointer(p,128)) return result;
    for(unsigned i=0;i<63;++i) {
      auto ch=rex::memory::load_and_swap<uint16_t>(memory+p+i*2);
      if(!ch) return result;
      if(ch<32||ch>126) return std::string{};
      result+=char(ch);
    }
    return std::string{};
  };
  const uint32_t parent=HudWord(memory,element+40);
  const auto id=name(HudWord(memory,element+4));
  if(!HudPointer(parent,32)||!pdz::hud::NativeHalfSizeElement(
      id,name(HudWord(memory,parent+4)))) return;
  const auto value=[memory,element](uint32_t offset) {
    return rex::memory::load_and_swap<float>(memory+element+offset);
  };
  const float x=value(44),y=value(48),sx=value(56),sy=value(60);
  const float w=value(12),h=value(16),px=value(84),py=value(88);
  for(float v:{x,y,sx,sy,w,h,px,py}) if(!std::isfinite(v)) return;
  base=memory; data=element;
  constexpr uint32_t offsets[]{44,48,56,60};
  for(unsigned i=0;i<4;++i) original[i]=HudWord(base,data+offsets[i]);
  rex::memory::store_and_swap<float>(base+data+44,pdz::hud::HalfSizePosition(x,sx,w,px));
  float scaled_y=pdz::hud::HalfSizePosition(y,sy,h,py);
  const auto anchor_id=pdz::hud::StackAnchor(id);
  if(!anchor_id.empty()) {
    uint32_t sibling=HudWord(base,parent+24);
    for(unsigned n=0;n<256&&HudPointer(sibling,240);++n) {
      if(name(HudWord(base,sibling+4))==anchor_id) {
        const auto f=[&](uint32_t offset){return rex::memory::load_and_swap<float>(base+sibling+offset);};
        const float anchor=f(48)+f(60)*(f(16)*.5f-f(88));
        if(std::isfinite(anchor)) scaled_y=anchor+.75f*(y-anchor);
        break;
      }
      const auto next=HudWord(base,sibling+32);
      if(next==sibling) break;
      sibling=next;
    }
  }
  rex::memory::store_and_swap<float>(base+data+48,scaled_y);
  rex::memory::store_and_swap<float>(base+data+56,sx*.75f);
  rex::memory::store_and_swap<float>(base+data+60,sy*.75f);
  rex::memory::store_and_swap<uint32_t>(base+data+236,HudWord(base,data+236)|2u);
}
PDZ_NativeHudScaleScope::~PDZ_NativeHudScaleScope() {
  if(!base) return;
  constexpr uint32_t offsets[]{44,48,56,60};
  for(unsigned i=0;i<4;++i) rex::memory::store_and_swap<uint32_t>(base+data+offsets[i],original[i]);
  // Recompute from original inputs on the next call, never halve an already-halved scale.
  rex::memory::store_and_swap<uint32_t>(base+data+236,HudWord(base,data+236)|2u);
}

bool PDZ_ReadClassicHud(pdz::hud::LiveState& state) {
  const int64_t last=classic_hud_render_time.load(std::memory_order_acquire);
  if(!last||HudTime()-last>250) return false;
  auto* runtime=rex::Runtime::instance();
  if(!ReadClassicHud(runtime?runtime->virtual_membase():nullptr,state,true)) return false;
  const int64_t health_last=classic_health_render_time.load(std::memory_order_acquire);
  state.health_alpha=pdz::hud::HealthOpacity(HudTime(),health_last,
      classic_health_alpha.load(std::memory_order_relaxed));
  return true;
}

namespace {
void RegisterScopeScene(uint8_t* base,uint32_t resource,uint32_t handle) {
  if(!base||!HudPointer(resource,192)||!handle) return;
  char name[96]{};
  for(unsigned i=0;i<95;++i) {
    const auto ch=rex::memory::load_and_swap<uint16_t>(base+resource+i*2);
    if(!ch) break;
    if(ch<32||ch>126) return;
    name[i]=char(ch);
  }
  std::lock_guard lock(scope_mutex);
  // Invalidate a reused table slot regardless of generation/resource type.
  for(auto it=scope_scenes.begin();it!=scope_scenes.end();)
    if((it->first&0xffff)==(handle&0xffff)) it=scope_scenes.erase(it); else ++it;
  uint32_t kind=UINT32_MAX;
  // Only shared basicscope needs equipment attribution. Keep unrelated/menu
  // scene callbacks on their existing cheap path.
  if(pdz::hud::VerifiedGenericScopeResource(name,2)) {
    const auto player=LocalPlayer(base);
    if(player) {
      const auto weapon=HudWord(base,player+4080);
      if(GuestReadable(base,weapon,1100)&&HudWord(base,weapon+92)==player) {
        const auto definition=HudWord(base,weapon+860);
        if(GuestReadable(base,definition,1148)) kind=HudWord(base,definition+1144);
      }
    }
  }
  if(pdz::hud::ConventionalScopeResource(name)||pdz::hud::VerifiedGenericScopeResource(name,kind))
    scope_scenes[handle]=pdz::hud::ScopeResourceName(name).starts_with("aid_darkxui_ingame_large_");
}
}
void PDZ_ObserveScopeScene(uint8_t* base,uint32_t resource,uint32_t output) {
  if(!base||!HudPointer(output,4)) return;
  RegisterScopeScene(base,resource,HudWord(base,output));
}

bool PDZ_ReadClassicScope(PDZ_ClassicScopeState& state) {
  std::lock_guard lock(scope_mutex);
  const auto age=HudTime()-scope_visible;
  // Keep ordinary hip-fire frames on the cheap no-scope path.
  if(!scope_visible||age<0||age>120)return false;
  auto* runtime=rex::Runtime::instance();
  auto* base=runtime?runtime->virtual_membase():nullptr;
  const auto player=LocalPlayer(base);
  if(!player||base[player+0x16A3]||DualHeld(base,player))return false;
  const auto root=HudWord(base,0x82E35468u);
  const auto weapon=HudWord(base,player+4080)?HudWord(base,player+4080):HudWord(base,player+4076);
  const uint64_t session=(uint64_t(root)<<32)|player;
  const bool ads_held=AimHeld(base,player);
  // A scope XUI render heartbeat alone is not authoritative: hidden/stale
  // aiming scenes can continue dispatching after ADS release or a reload.
  if(!pdz::hud::ScopePresentationAllowed(scope_visible&&age>=0&&age<=120,
       scope_session==session,weapon&&scope_weapon==weapon,scope_state.aiming,ads_held))return false;
  state=scope_state; return true;
}

bool PDZ_SuppressClassicHudRender(uint8_t* base, uint32_t handle, uint32_t message) {
  // Native XUI message 0 is rendering: Figure/TextPresenter dispatch it to
  // shape/text drawing; XuiElement dispatches it to transform/child rendering.
  // Other messages (updates, properties, input, creation and destruction) pass.
  // Scope scene observation is always needed for the permanent scoped-assist
  // ban, even when the replacement artwork and all HUD enhancements are off.
  if(!base||!HudPointer(message,20)||HudWord(base,message+4u)!=0u) return false;
  const uint32_t count=HudWord(base,0x82D35768u);
  const uint32_t table=HudWord(base,0x82D35760u);
  const uint32_t index=handle&0xFFFFu;
  if(count>16384u||index>=count||!HudPointer(table,count*12u)) return false;
  const uint32_t entry=table+index*12u;
  if(rex::memory::load_and_swap<uint16_t>(base+entry)!=(handle>>16)||!(base[entry+3u]&1u)) return false;
  // XuiScene instance+24 retains the actual asset URI, verified live. Read it
  // directly rather than depending on one of several native creation callers.
  // Generic basicscope additionally requires a verified weapon kind; scene
  // names alone are not sufficient evidence to suppress a gadget or RCP sight.
  uint32_t scene_part=HudWord(base,entry+4u);
  uint32_t scope_handle=handle;
  // Every inherited XUI component has its OWN handle. The rendered base
  // XuiElement's forward chain can be empty; +12 links to the logical owner
  // head. Verified scoped pistol: Scene40532 -> Control40533 -> Element40534,
  // all +12==Scene head. Resolve that head with registry/generation checks.
  if(HudPointer(scene_part,32)&&HudWord(base,scene_part)==handle) {
    const auto owner=HudWord(base,scene_part+12);
    if(HudPointer(owner,32)) {
      const auto owner_handle=HudWord(base,owner),owner_index=owner_handle&0xffffu;
      if(owner_index<count) {
        const auto owner_entry=table+owner_index*12;
        if(pdz::hud::ValidScopeOwner(owner_handle,owner,HudWord(base,owner_entry+4),
             rex::memory::load_and_swap<uint16_t>(base+owner_entry),
             (base[owner_entry+3]&1)!=0,HudWord(base,owner+12))) {
          scene_part=owner;
          scope_handle=owner_handle;
        }
      }
    }
  }
  for(unsigned depth=0;depth<16&&HudPointer(scene_part,32);++depth) {
    if(HudWord(base,scene_part+24)==0x8276C6F0u) {
      const auto scene_data=HudWord(base,scene_part+28);
      if(HudPointer(scene_data,28)) RegisterScopeScene(base,HudWord(base,scene_data+24),scope_handle);
      break;
    }
    const auto next=HudWord(base,scene_part+4);
    if(next==scene_part) break;
    scene_part=next;
  }
  {
    std::lock_guard lock(scope_mutex);
    if(const auto scene=scope_scenes.find(scope_handle);scene!=scope_scenes.end()) {
      pdz::hud::LiveState live;
      if(!ReadClassicHud(base,live,false)||pdz::UiMouseCaptured()) return false;
      const auto root=HudWord(base,0x82E35468u),player=HudWord(base,root+0x3B8);
      const auto weapon=HudWord(base,player+4080)?HudWord(base,player+4080):HudWord(base,player+4076);
      const uint64_t session=(uint64_t(root)<<32)|player;
      if(session!=scope_session) { scope_session=session;scope_selected.clear(); }
      const auto manager=HudWord(base,0x82D68320u);
      if(!HudPointer(manager,0x444)||!weapon||DualHeld(base,player)) return false;
      const float normal=rex::memory::load_and_swap<float>(base+0x820EC228u);
      const float current=rex::memory::load_and_swap<float>(base+manager+0x440);
      if(!std::isfinite(normal)||!std::isfinite(current)||normal<=0||current<=0) return false;
      scope_state.aiming=scene->second;
      scope_state.current_zoom=std::clamp(normal/current,1.f,100.f);
      if(scope_state.aiming&&AimHeld(base,player)) scope_selected[weapon]=scope_state.current_zoom;
      scope_state.selected_zoom=scope_selected.contains(weapon)?scope_selected[weapon]:1.f;
      scope_weapon=weapon;scope_visible=HudTime();
      return rex::cvar::Query<bool>("pdz_classic_scope");
    }
  }
  uint32_t part=HudWord(base,entry+4u);
  for(int depth=0;depth<16&&HudPointer(part,32);++depth) {
    // Base XuiElement component callback, verified through native class metadata.
    if(HudWord(base,part+24u)==0x8276C628u) {
      const uint32_t data=HudWord(base,part+28u);
      if(!HudPointer(data,24)) return false;
      const uint32_t text=HudWord(base,data+4u);
      if(!HudPointer(text,64)) return false;
      char id[32]{};
      for(unsigned i=0;i<31;++i) {
        const uint16_t ch=rex::memory::load_and_swap<uint16_t>(base+text+i*2u);
        if(!ch) break;
        if(ch<32||ch>126) return false;
        id[i]=char(ch);
      }
      const bool replacement=pdz::hud::ReplacedElement(id)&&rex::cvar::Query<bool>("pdz_classic_hud");
      const bool objective=pdz::hud::ObjectiveElement(id)&&
          rex::cvar::Query<bool>("pdz_classic_hud_hide_objective");
      const bool interaction=pdz::hud::InteractionPromptElement(id)&&
          rex::cvar::Query<bool>("pdz_hide_interaction_prompts");
      if(!replacement&&!objective&&!interaction) return false;
      pdz::hud::LiveState state;
      if(!ReadClassicHud(base,state,false)) return false;
      if(replacement) classic_hud_render_time.store(HudTime(),std::memory_order_release);
      if(pdz::hud::HealthElement(id)) {
        const float alpha=rex::memory::load_and_swap<float>(base+data+20u);
        if(std::isfinite(alpha)&&alpha>0.f) {
          classic_health_alpha.store(std::clamp(alpha,0.f,1.f),std::memory_order_relaxed);
          classic_health_render_time.store(HudTime(),std::memory_order_release);
        }
      }
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
      static std::mutex log_mutex;
      static std::unordered_set<std::string> logged;
      {
        const std::lock_guard lock(log_mutex);
        if(logged.insert(id).second) REXLOG_INFO("PDZ Classic HUD: native render suppressed for {} (handle={:08X})",id,handle);
      }
#endif
      return true;
    }
    const uint32_t next=HudWord(base,part+4u);
    if(next==part) break;
    part=next;
  }
  return false;
}

// Bounded read-only attribution capture: no guest registers, buttons, draw
// state or visibility are changed. Only the known global HUD sprite batch.
void PDZ_ObserveHudDraw(uint8_t* base, uint32_t batch, uint32_t position,
                       uint32_t color, uint32_t tile, uint32_t flags,
                       uint32_t caller, double width, double height) {
  if (!base || position < 0x10000u || position > UINT32_MAX-12u) return;
  const auto load = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base+address);
  };
  const uint32_t root=load(0x82E35468u);
  if(root<0x10000u||root>UINT32_MAX-0x3BCu) return;
  const uint32_t player=load(root+0x3B8u);
  if(player<0x10000u||player>UINT32_MAX-0x2000u||load(player)!=0x820C93A0u||
     base[player+0x16A3u]||pdz::UiMouseCaptured()) return;
  const uint32_t hud=load(0x82D6B2E4u);
  if(hud<0x10000u||hud>UINT32_MAX-8u||batch!=load(hud+4u)) return;
  static std::mutex mutex;
  static std::unordered_set<uint64_t> seen;
  static int64_t start=0;
  const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  const std::lock_guard guard(mutex);
  if(!start) {
    start=now;
    REXLOG_INFO("PDZ HUD attribution capture active: player={:08X} batch={:08X}; bounded to 8 seconds/256 identities",player,batch);
  }
  if(now-start>8000||seen.size()>=256) return;
  const uint64_t key=(uint64_t(caller)<<32)|tile;
  if(!seen.insert(key).second) return;
  const float x=rex::memory::load_and_swap<float>(base+position);
  const float y=rex::memory::load_and_swap<float>(base+position+4u);
  REXLOG_INFO("PDZ HUD sprite: caller={:08X} tile={} flags={:08X} color={:08X} xy=({}, {}) size=({}, {})",
              caller,tile,flags,color,x,y,width,height);
}

namespace {
std::atomic<uint32_t> zoom_release_generation{0};
}

uint32_t PDZ_ScopeAllowed(uint8_t* base,uint32_t player,uint32_t original) {
  if(!base||!HudPointer(player,8208)||HudWord(base,player)!=0x820C93A0u) return original;
  const uint32_t root=HudWord(base,0x82E35468u);
  if(!HudPointer(root,0x3BC)||HudWord(base,root+0x3B8)!=player) return original;
  return DualHeld(base,player)?0:original;
}

void PDZ_RecordWeaponInput(uint8_t* base, uint32_t state, uint32_t user, uint32_t result) {
  if (user != 0) return;
  uint32_t packed = 0;
  if (!result && base && state >= 0x10000u && state <= UINT32_MAX - 16u) {
    packed = rex::memory::load_and_swap<uint16_t>(base + state + 4u);
    packed |= uint32_t(base[state + 6u]) << 16;
  }
  const uint32_t previous = weapon_input.exchange(packed);
  weapon_right_trigger.store(!result&&base&&HudPointer(state,16)?base[state+7]:0);
  // RT (including mapped mouse fire) keeps the HUD up even for empty clicks;
  // reload press wakes it immediately, before magazine counts change.
  if(!result&&base&&state>=0x10000u&&state<=UINT32_MAX-16u&&
     !pdz::UiMouseCaptured()&&(base[state+7u]>=30||((packed&0x4000u)&&!(previous&0x4000u))))
    ammo_input_activity_time.store(HudTime(),std::memory_order_relaxed);
  if ((previous >> 16) >= 30 && (packed >> 16) < 30) {
    zoom_release_generation.fetch_add(1);
    zoom_wheel.store(0);
  }
  // Preserve raw D-pad above for zoom, then consume only horizontal D-pad
  // in the game's copy. Never alter LT, other buttons or failed input results.
  static std::mutex hold_mutex;
  static pdz::HolsterHold hold;
  static uint32_t last_packet=0, filtered_packet=0;
  static uint16_t last_routed=0;
  if(result||!base||!HudPointer(state,16)) {
    std::lock_guard lock(hold_mutex);hold={}; return;
  }
  const bool hold_enabled=rex::cvar::Query<bool>("pdz_hold_y_holster");
  uint16_t buttons=uint16_t(packed);
  {
    static std::mutex action_mutex;
    static pdz::input::ReloadInteract action;
    static uint32_t action_packet=0;
    const auto player=LocalPlayer(base);
    const bool gameplay=player&&!base[player+0x16A3]&&FirstPerson(base)&&!pdz::UiMouseCaptured();
    bool known=false,can_reload=false;
    if(gameplay&&(buttons&0x4000u)&&!DualHeld(base,player)) {
      const auto weapon=HudWord(base,player+4080u);
      if(EquipmentStatus(base,player)==EquippedStatus::Unarmed) known=true;
      else if(GuestReadable(base,weapon,1330)&&HudWord(base,weapon+92)==player) {
        const auto definition=HudWord(base,weapon+860u);
        if(GuestReadable(base,definition,200)) {
          const auto half=[base](uint32_t p){return rex::memory::load_and_swap<uint16_t>(base+p);};
          const auto loaded=half(weapon+1316),capacity=half(definition+154);
          const auto type=base[definition+199];
          const auto pool=HudWord(base,weapon+1100);
          if(type<=15&&capacity<=1000&&(!pool||GuestReadable(base,pool,84))) {
            const auto reserve=pool?half(pool+(26u+type)*2u):half(weapon+1318);
            known=true; can_reload=loaded<capacity&&reserve>0;
          }
        }
      }
    }
    std::lock_guard lock(action_mutex);
    const bool action_enabled=rex::cvar::Query<bool>("pdz_reload_interact");
    buttons=action.Route(buttons,player,action_enabled,gameplay,known,can_reload,HudTime());
    if(action_enabled&&gameplay) {
      rex::memory::store_and_swap<uint16_t>(base+state+4,buttons);
      // A press/release pulse may change across polls of one hardware packet.
      // Give every routed poll a fresh counter so release cannot remain cached.
      rex::memory::store_and_swap<uint32_t>(base+state,++action_packet);
    }
  }
  if(hold_enabled) {
    const auto player=LocalPlayer(base);
    const bool gameplay=player&&!base[player+0x16A3]&&!pdz::UiMouseCaptured();
    std::lock_guard lock(hold_mutex);
    buttons=hold.Route(buttons,true,gameplay,
        gameplay&&EquipmentStatus(base,player)==EquippedStatus::Unarmed,
        player,HudTime());
    if(gameplay) {
      const auto packet=HudWord(base,state);
      if(packet!=last_packet||buttons!=last_routed) ++filtered_packet;
      last_packet=packet;last_routed=buttons;
      rex::memory::store_and_swap<uint32_t>(base+state,filtered_packet);
    }
    rex::memory::store_and_swap<uint16_t>(base+state+4,buttons);
  } else {
    std::lock_guard lock(hold_mutex);hold={};
  }
  if(!rex::cvar::Query<bool>("pdz_dpad_zoom")||pdz::UiMouseCaptured())return;
  const auto load = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  const uint32_t root = load(0x82E35468u);
  if (root < 0x10000u || root > UINT32_MAX - 0x3BCu) return;
  const uint32_t player = load(root + 0x3B8u);
  if (player < 0x10000u || player > UINT32_MAX - 0x2000u ||
      load(player) != 0x820C93A0u || base[player + 0x16A3u]) return;
  if(DualHeld(base,player)) return;
  const uint16_t routed = pdz::RouteZoomButtons(buttons, uint8_t(packed >> 16), true, true);
  rex::memory::store_and_swap<uint16_t>(base + state + 4u, routed);
}

void PDZ_AdjustWeaponBob(uint8_t* base,uint32_t player,uint32_t parameters,
                         double& x,double& y) {
  const double amount=rex::cvar::Query<double>("pdz_weapon_bob_amount");
  if(amount==1||LocalPlayer(base)!=player||!FirstPerson(base)||
     !GuestReadable(base,parameters,56))return;
  // At825A40AC f25/f26 contain the neutral negative weapon offsets plus
  // procedural sin/cos motion. Later attachment offsets, rotations (including
  // Classic aim sway) and skeletal action animations remain native.
  const double neutral_x=-rex::memory::load_and_swap<float>(base+parameters+44);
  const double neutral_y=-rex::memory::load_and_swap<float>(base+parameters+48);
  x=pdz::BobAmplitude(x,neutral_x,amount);
  y=pdz::BobAmplitude(y,neutral_y,amount);
}

uint32_t PDZ_IdleFistVisibility(uint8_t* base,uint32_t player,uint32_t native_mask) {
  if(!(native_mask&1u)||!rex::cvar::Query<bool>("pdz_hide_unarmed_hands")||
     LocalPlayer(base)!=player||!FirstPerson(base)||base[player+0x16A3]||
     HudWord(base,player+4076)||HudWord(base,player+4080)||
     (weapon_input.load()>>16)>=30||weapon_right_trigger.load()>=30)return native_mask;
  const auto body=HudWord(base,player+72);
  if(!GuestReadable(base,body,8)||HudWord(base,body+4)!=player+5524)return native_mask;
  // Exact linked-list layout used by native821E4578. Inspect every active
  // locomotion/overlay, including duplicate/outgoing attack clips during blending.
  const auto first=HudWord(base,player+5536),tail=HudWord(base,player+5544);
  if(!tail||tail<first||(tail-first)%8||(tail-first)/8>=256)return native_mask;
  int index=int((tail-first)/8);
  bool seen[256]{}, idle=false;
  while(index!=-1) {
    if(index<0||index>=256||seen[index])return native_mask;
    seen[index]=true;
    const auto entry=first+uint32_t(index)*8;
    if(!GuestReadable(base,entry,8))return native_mask;
    const auto slot=HudWord(base,entry+4);
    if(!GuestReadable(base,slot,248))return native_mask;
    const auto channel=base[slot];
    if(channel!=0) {
      const bool fist_channel=channel==base[player+4112]||channel==base[player+4113]||
                              channel==base[player+4114];
      if(!fist_channel&&(channel<1||channel>4))return native_mask;
      const auto descriptor=HudWord(base,slot+244);
      if(!GuestReadable(base,descriptor,32))return native_mask;
      const auto name=HudWord(base,descriptor);
      if(!GuestReadable(base,name,96))return native_mask;
      const auto chars=reinterpret_cast<const char*>(base+name);
      const auto end=static_cast<const char*>(std::memchr(chars,0,96));
      if(!end)return native_mask;
      const std::string_view clip(chars,size_t(end-chars));
      const bool recognized=(fist_channel&&pdz::VerifiedIdleFists(clip))||
                             pdz::VerifiedUnarmedLocomotion(clip,channel);
      if(!recognized)return native_mask;
      idle=true;
    }
    index=int16_t(rex::memory::load_and_swap<uint16_t>(base+entry+2));
  }
  if(!idle)return native_mask;
  static std::atomic<bool> reported{false};
  if(!reported.exchange(true))REXLOG_INFO("PDZ cached passive fist visibility active: player={:08X}, verified idle/locomotion; attack/blend masks preserved",player);
  // Native r21/r22 feed cached first-person meshes, even when held item slots
  // are null. Mesh5272 was already invisible in the failed idle snapshot.
  // Preserve other view bits and the separate body/head/attachment masks.
  return pdz::FistViewMask(native_mask,true);
}

void PDZ_ObserveUnarmedAction(uint8_t* base,uint32_t state,uint32_t action,uint32_t success) {
  if(!(success&255)||!GuestReadable(base,state,32))return;
  const auto player=LocalPlayer(base),item=HudWord(base,state+28);
  if(!player||EquipmentStatus(base,player)!=EquippedStatus::Unarmed||
     !GuestReadable(base,item,1100))return;
  const auto model=HudWord(base,item+1096);
  if(!GuestReadable(base,model,220))return;
  const int hand=HudWord(base,player+4076)==item?0:
      HudWord(base,player+4080)==item?1:-1;
  if(hand<0)return;
  std::lock_guard lock(unarmed_action_mutex);
  if(unarmed_action_owner!=player) {
    unarmed_actions[0]={};unarmed_actions[1]={};unarmed_action_owner=player;
  }
  unarmed_actions[hand]={item,model,HudWord(base,model+40),action};
}

void PDZ_AdjustUnarmedViewmodel(uint8_t* base,uint32_t player,uint32_t item,uint32_t matrix) {
  if(!rex::cvar::Query<bool>("pdz_hide_unarmed_hands")||
     LocalPlayer(base)!=player||!FirstPerson(base)||base[player+0x16A3]||
     EquipmentStatus(base,player)!=EquippedStatus::Unarmed||
     !GuestReadable(base,item,1100)||!GuestReadable(base,matrix,64))return;
  const auto model=HudWord(base,item+1096);
  if(!GuestReadable(base,model,220))return;
  UnarmedAction action;
  {
    std::lock_guard lock(unarmed_action_mutex);
    if(unarmed_action_owner==player)
      for(const auto& candidate:unarmed_actions)
        if(candidate.item==item&&candidate.model==model)action=candidate;
  }
  const bool known=action.item&&action.clip==HudWord(base,model+40);
  const double phase=rex::memory::load_and_swap<float>(base+model+196);
  const double blend=rex::memory::load_and_swap<float>(base+model+32);
  const bool blending=base[model+28]&&(!std::isfinite(blend)||blend<1);
  const bool trigger=(weapon_input.load()>>16)>=30||weapon_right_trigger.load()>=30;
  if(pdz::ShowUnarmedHands(action.action,known,trigger,action.clip!=0,phase,blending))return;
  // Only the temporary first-person submission matrix: native recomputes it
  // every frame. Never move the player, camera, collision or third-person body.
  float translation[3];
  for(unsigned i=0;i<3;++i) {
    translation[i]=rex::memory::load_and_swap<float>(base+matrix+48+i*4);
    if(!std::isfinite(translation[i]))return;
  }
  for(unsigned i=0;i<3;++i)
    rex::memory::store_and_swap<float>(base+matrix+48+i*4,translation[i]+1000000.f);
}

void PDZ_QueueZoomWheel(float amount) {
  if(!std::isfinite(amount)||amount==0||pdz::UiMouseCaptured()||
     !rex::cvar::Query<bool>("pdz_wheel_zoom")) return;
  auto* rt=rex::Runtime::instance();auto* base=rt?rt->virtual_membase():nullptr;
  if(!base) return;
  const uint32_t root=HudWord(base,0x82E35468u);
  if(!HudPointer(root,0x3BC)) return;
  const uint32_t player=HudWord(base,root+0x3B8);
  if(!HudPointer(player,8208)||HudWord(base,player)!=0x820C93A0u||
     base[player+0x16A3]||!AimHeld(base,player)) return;
  const int steps=std::clamp(int(std::trunc(std::clamp(amount,-3.f,3.f))),-3,3);
  if(!steps) return;
  int previous=zoom_wheel.load();
  while(!zoom_wheel.compare_exchange_weak(previous,std::clamp(previous+steps,-3,3))) {}
  zoom_wheel_time.store(HudTime());
}
double PDZ_DpadZoomPosition(uint8_t* base, uint32_t player, double original,
                            double seconds, bool allowed) {
  static std::mutex lock;
  static pdz::WeaponZoomMemory memory;
  static uint32_t generation = 0;
  const std::lock_guard guard(lock);
  if (!base || player < 0x10000u || player > UINT32_MAX - 0x2000u) return original;
  const auto load = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  const uint32_t root = load(0x82E35468u);
  if (root < 0x10000u || root > UINT32_MAX - 0x3BCu) { memory.Reset(); return original; }
  const uint32_t local_player=load(root+0x3B8u);
  if(!local_player) { memory.Reset(); return original; }
  if(local_player!=player || load(player)!=0x820C93A0u) return original;
  const uint32_t input = weapon_input.load();
  const uint32_t release = zoom_release_generation.load();
  const uint32_t weapon = load(player + 4080u)?load(player+4080u):load(player+4076u);
  auto& zoom=memory.Select((uint64_t(root)<<32)|player,weapon);
  if(release != generation) zoom.EndGesture();
  generation = release;
  const bool dpad = rex::cvar::Query<bool>("pdz_dpad_zoom");
  const bool enabled = dpad || rex::cvar::Query<bool>("pdz_wheel_zoom");
  const int queued_wheel=zoom_wheel.exchange(0);
  const int wheel=HudTime()-zoom_wheel_time.load()<=250?queued_wheel:0;
  if (!std::isfinite(seconds) || seconds <= 0 || seconds > .25) {
    zoom.EndGesture();
    return original;
  }
  const bool aiming = allowed && !DualHeld(base,player) && (input >> 16) >= 30;
  // Seed at two thirds; retain the requested stage across ADS releases.
  const double position = zoom.Update(enabled, aiming, !pdz::UiMouseCaptured(),
                                      dpad&&(input & 4u) != 0, dpad&&(input & 8u) != 0,
                                      0., seconds,rex::cvar::Query<double>("pdz_zoom_transition"),
                                      aiming&&rex::cvar::Query<bool>("pdz_wheel_zoom")?wheel:0);
  if (!enabled || !aiming || pdz::UiMouseCaptured() || !std::isfinite(seconds) ||
      seconds < 0 || seconds > .25) return original;
  static std::atomic<bool> reported{false};
  if (!reported.exchange(true))
    REXLOG_INFO("PDZ D-pad zoom active: player={:08X} pressure={} position={}", player, original, position);
  return position;
}

namespace {
thread_local pdz::DodgeRollFatigue dodge_fatigue;
thread_local uint32_t dodge_fatigue_player = 0, dodge_fatigue_config = 0;

void ObserveDodgeRoll(uint8_t* base, uint32_t player, uint32_t config, double seconds) {
  const auto valid = [](uint32_t address, uint32_t bytes) {
    return address >= 0x10000u && address <= UINT32_MAX - bytes;
  };
  const auto load = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  if (!base) return;
  const uint32_t root = load(0x82E35468u);
  if (!valid(root, 0x3BCu) || load(root + 0x3B8u) != player) return;
  if (!rex::cvar::Query<bool>("pdz_dodge_roll")) {
    dodge_fatigue.Reset();
    return;
  }
  if (player != dodge_fatigue_player || config != dodge_fatigue_config) {
    dodge_fatigue.Reset();
    dodge_fatigue_player = player;
    dodge_fatigue_config = config;
  }
  bool rolling = false;
  uint32_t descriptor = 0;
  double phase = 0.;
  if (valid(player, 232u) && load(player) == 0x820C93A0u) {
    const uint32_t manager = load(player + 72u);
    const uint32_t data = valid(manager, 8u) ? load(manager + 4u) : 0;
    descriptor = valid(data, 48u) ? load(data + 44u) : 0;
    if (valid(descriptor, 12u) && pdz::IsDodgeRollAnimation(load(descriptor + 8u))) {
      PPCContext lookup{};
      lookup.r3.u32 = data + 12u;
      lookup.r4.u32 = 0;
      sub_821E4578(lookup, base);
      const uint32_t slot = lookup.r3.u32;
      if (valid(slot, 248u) && load(slot + 244u) == descriptor) {
        phase = rex::memory::load_and_swap<float>(base + slot + 164u);
        rolling = true;
      }
    }
  }
  dodge_fatigue.Observe(seconds, rolling, phase, descriptor);
}
} // namespace

void PDZ_ObserveDodgeRoll(uint8_t* base,uint32_t player,uint32_t config,double seconds) {
  ObserveDodgeRoll(base,player,config,seconds);
}

#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
bool PDZ_TraceDodgeRoute(uint8_t* base, uint32_t player, uint32_t config,
                         double seconds, unsigned stage) {
  if (!base || stage > 4 || player < 0x10000u || player > UINT32_MAX - 0x16BAu) return false;
  const auto load = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  const uint32_t root = load(0x82E35468u);
  if (root < 0x10000u || root > UINT32_MAX - 0x3BCu ||
      load(root + 0x3B8u) != player) return false;
  const uint32_t manager = load(player + 72u);
  if (manager < 0x10000u || manager > UINT32_MAX - 8u) return false;
  const uint32_t data = load(manager + 4u);
  if (data < 0x10000u || data > UINT32_MAX - 48u) return false;
  const uint32_t descriptor = load(data + 44u);
  if (descriptor < 0x10000u || descriptor > UINT32_MAX - 12u ||
      !pdz::IsDodgeRollAnimation(load(descriptor + 8u))) return false;
  static std::atomic<bool> reported[5]{};
  if (!reported[stage].exchange(true))
    REXLOG_INFO("PDZ dodge route stage={} player={:08X} config={:08X} body={:08X} dt={} animation={} enabled={}",
                stage, player, config, load(player + 228u), seconds,
                load(descriptor + 8u), rex::cvar::Query<bool>("pdz_dodge_roll"));
  return true;
}
#endif

void PDZ_AdjustDodgeRollDelta(uint8_t* base, uint32_t player, uint32_t config,
                             double seconds, double& x, double& z) {
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  const bool traced_roll = PDZ_TraceDodgeRoute(base, player, config, seconds, 2);
  const auto rejected = [&](unsigned reason) {
    static std::atomic<bool> reported[3]{};
    if (traced_roll && !reported[reason].exchange(true))
      REXLOG_INFO("PDZ dodge adjustment rejected: reason={} dt={} X={} Z={} config={:08X}",
                  reason, seconds, x, z, config);
  };
#else
  const auto rejected = [](unsigned) {};
#endif
  if (!base || !rex::cvar::Query<bool>("pdz_dodge_roll") ||
      !std::isfinite(seconds) || seconds <= 0 || seconds > .25 ||
      !std::isfinite(x) || !std::isfinite(z) || std::hypot(x, z) > 500.) { rejected(0); return; }
  const auto valid = [](uint32_t address, uint32_t bytes) {
    return address >= 0x10000u && address <= UINT32_MAX - bytes;
  };
  const auto load = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  const uint32_t root = load(0x82E35468u);
  if (!valid(root, 0x3BCu) || load(root + 0x3B8u) != player ||
      !valid(player, 0x16BAu)) return;
  const uint32_t body = load(player + 228u);
  if (!valid(body, 408u)) return;
  if (!valid(player, 0x16BAu) || load(player) != 0x820C93A0u ||
      load(body) != 0x820CC6B0u || body + 16u != config ||
      rex::memory::load_and_swap<uint8_t>(base + player + 0x16B9u) != 0) { rejected(1); return; }
  const uint32_t manager = load(player + 72u);
  if (!valid(manager, 8u)) return;
  const uint32_t data = load(manager + 4u);
  if (!valid(data, 48u)) return;
  const uint32_t descriptor = load(data + 44u);
  if (!valid(descriptor, 12u) || !pdz::IsDodgeRollAnimation(load(descriptor + 8u))) return;
  // Use the engine's slot lookup, not the non-atomic external recorder's walk.
  PPCContext lookup{};
  lookup.r3.u32 = data + 12u;
  lookup.r4.u32 = 0;
  sub_821E4578(lookup, base);
  const uint32_t slot = lookup.r3.u32;
  if (!valid(slot, 248u) || load(slot + 244u) != descriptor) { rejected(2); return; }
  // Native821E4668 advances slot+16's +148 by rate*dt, and8270D6C0
  // writes it back. Captured roll clips run from 0 to 1 on this same field.
  const double phase = rex::memory::load_and_swap<float>(base + slot + 164u);
  // Animation can enter a roll between the movement observer and this
  // integrator. Select its tier immediately, without advancing time twice.
  if (dodge_fatigue_player != player || dodge_fatigue_config != config) {
    dodge_fatigue.Reset();
    dodge_fatigue_player = player;
    dodge_fatigue_config = config;
  }
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  const double previous_start = dodge_fatigue.last_start;
#endif
  dodge_fatigue.Observe(0., true, phase, descriptor);
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  if (dodge_fatigue.last_start != previous_start) {
    static std::atomic<unsigned> roll_reports{0};
    if (roll_reports.fetch_add(1) < 20)
      REXLOG_INFO("PDZ dodge fatigue: tier={} strength={} gameplay_time={:.3f}",
                  dodge_fatigue.tier, dodge_fatigue.strength, dodge_fatigue.time);
  }
#endif
  const double strength = dodge_fatigue_player == player && dodge_fatigue_config == config
      ? dodge_fatigue.strength : 1.6;
  const double scale = pdz::DodgeRollScale(true, phase, strength);
  static std::atomic<bool> checked{false};
  if (!checked.exchange(true))
    REXLOG_INFO("PDZ dodge roll validated: phase={} strength={} scale={} X={} Z={}",
                phase, strength, scale, x, z);
  if (scale == 1.) return;
  const double original_x = x, original_z = z;
  // World-space per-frame animation displacement, before native integration.
  // Do not alter accumulated animation translation or walking requests.
  x = double(float(x * scale));
  z = double(float(z * scale));
  static std::atomic<bool> reported{false};
  if ((x != 0 || z != 0) && !reported.exchange(true))
    REXLOG_INFO("PDZ dodge roll root displacement active: player={:08X} animation={} phase={:.4f} scale={:.4f} X={:.4f}->{:.4f} Z={:.4f}->{:.4f}",
                player, load(descriptor + 8u), phase, scale, original_x, x, original_z, z);
}

double PDZ_AdjustDodgeRollVelocity(uint8_t* base, uint32_t player, uint32_t config,
                                  double seconds, double& x, double& z) {
  if (!std::isfinite(seconds) || seconds <= 0 || seconds > .25 ||
      !std::isfinite(x) || !std::isfinite(z)) return 1.;
  const double original_x = x * seconds, original_z = z * seconds;
  double delta_x = original_x, delta_z = original_z;
  PDZ_AdjustDodgeRollDelta(base, player, config, seconds, delta_x, delta_z);
  // Return exactly 1 without even a rounding change when disabled/rejected.
  if (delta_x == original_x && delta_z == original_z) return 1.;
  const double scale = std::abs(original_x) >= std::abs(original_z)
      ? delta_x / original_x : delta_z / original_z;
  if (!std::isfinite(scale) || scale < 1. || scale > 3.11) return 1.;
  x *= scale;
  z *= scale;
  return scale;
}

namespace {
std::atomic<int64_t> crosshair_last_visible_ms{0};
int64_t CrosshairClockMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace

double PDZ_WeaponActionRate(uint8_t* base, uint32_t state,
                            uint32_t action, double original) {
  if (!base || state < 0x10000u ||
      state > UINT32_MAX - 32u) return original;
  const auto load = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  const uint32_t root = load(0x82E35468u);
  const uint32_t entity = load(state + 28u);
  if (root < 0x10000u || root > UINT32_MAX - 0x3BCu ||
      entity < 0x10000u || entity > UINT32_MAX - 96u) return original;
  const uint32_t player = load(root + 0x3B8u);
  if (player < 0x10000u || load(player) != 0x820C93A0u ||
      load(entity + 92u) != player) return original;
  if(!pdz::IsFastWeaponAction(action)) {
    static std::atomic<bool> observed[27]{};
    if(action<27&&!observed[action].exchange(true))
      REXLOG_INFO("PDZ additional local item animation: entity={:08X} class={:08X} state={:08X} action={} rate={:.6f}",entity,load(entity),state,action,original);
    return original; // Extra clips may be use/fire/idle; never accelerate by guess.
  }
  const double speed = rex::cvar::Query<double>("pdz_weapon_action_speed");
  const double adjusted = pdz::WeaponActionRate(action, original, speed);
  static std::atomic<bool> reported[26]{};
  if (adjusted != original && !reported[action].exchange(true))
    REXLOG_INFO("PDZ weapon action speed active: player={:08X} state={:08X} action={} speed={} rate={:.6f}->{:.6f}",
                player, state, action, speed, original, adjusted);
  return adjusted;
}

double PDZ_ClassicNpcHealthLoss(uint8_t* base, uint32_t character, double original) {
  if (!base || !rex::cvar::Query<bool>("pdz_classic_npc_durability") ||
      character < 0x10000u || character > UINT32_MAX - 4u) return original;
  const auto load = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  const uint32_t table = load(character);
  if (table < 0x82000000u || table > 0x821FFFFFu - 816u ||
      !pdz::IsClassicNpcHealthPath(load(table + 812u), load(table + 332u)))
    return original;
  const double adjusted = pdz::ClassicNpcHealthLoss(original, true,
      rex::cvar::Query<double>("pdz_npc_health_scale"));
  static std::atomic<bool> reported{false};
  if (adjusted != original && !reported.exchange(true))
    REXLOG_INFO("PDZ Classic NPC health loss active: character={:08X} vtable={:08X} damage={:.4f}->{:.4f}",
                character, table, original, adjusted);
  return adjusted;
}

double PDZ_ClassicEquipmentFactor(uint8_t* base, uint32_t player,
                                  uint32_t caller, double original) {
  if (!base || caller != 0x8259EC3Cu ||
      !rex::cvar::Query<bool>("pdz_classic_movement")) return original;
  const auto load = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  const uint32_t root = load(0x82E35468u);
  if (root < 0x10000u || root > UINT32_MAX - 0x3BCu ||
      player < 0x10000u || player > UINT32_MAX - 0x16BAu ||
      load(root + 0x3B8u) != player || load(player) != 0x820C93A0u ||
      rex::memory::load_and_swap<uint8_t>(base + player + 0x16B9u) != 0)
    return original;
  // Equipment selection and the hit-timer substitution, never the combined cap.
  return pdz::ClassicEquipmentFactor(original, true);
}

double PDZ_ClassicTargetScale(uint8_t* base, uint32_t player, uint32_t input,
                              double seconds, uint32_t axis) {
  static std::mutex mutex;
  static pdz::ClassicRunState state;
  static uint32_t owner = 0;
  static int64_t last_ms = 0;
  static double scale = .8;
  std::lock_guard lock(mutex);
  const auto load = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  const uint32_t root = base ? load(0x82E35468u) : 0;
  if (!base || !rex::cvar::Query<bool>("pdz_classic_movement") || axis > 1 ||
      root < 0x10000u || root > UINT32_MAX - 0x3BCu ||
      player < 0x10000u || player > UINT32_MAX - 0x16BAu ||
      input < 0x10000u || input > UINT32_MAX - 1188u ||
      load(root + 0x3B8u) != player || load(player) != 0x820C93A0u ||
      rex::memory::load_and_swap<uint8_t>(base + player + 0x16B9u) != 0) {
    state = {}; owner = 0; last_ms = 0;
    return 1.;
  }
  if (axis == 0) {
    const int64_t now = CrosshairClockMs();
    if (owner != player || now - last_ms > 250) state = {};
    owner = player; last_ms = now;
    const bool enabled = int32_t(load(input + 1152u)) >= 0;
    const double forward = enabled ? rex::memory::load_and_swap<float>(base + input + 1184u) : 0;
    const double strafe = enabled ? rex::memory::load_and_swap<float>(base + input + 1180u) : 0;
    const bool fast=rex::cvar::Query<bool>("pdz_unarmed_run_boost")&&
        EquipmentStatus(base,player)==EquippedStatus::Unarmed;
    scale = pdz::ClassicRunScale(state, seconds, forward, strafe,fast);
  }
  return scale;
}

double PDZ_AdjustMovementRetention(uint8_t* base, uint32_t player,
                                   double original, uint32_t axis, double seconds,
                                   double old, double target) {
  const bool classic = rex::cvar::Query<bool>("pdz_classic_movement");
  if (!base || (!classic && !rex::cvar::Query<bool>("pdz_modern_movement"))) return original;
  const auto load = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  const uint32_t root = load(0x82E35468u);
  if (root < 0x10000u || root > UINT32_MAX - 0x3BCu ||
      player < 0x10000u || player > UINT32_MAX - 0x16BAu ||
      load(root + 0x3B8u) != player || load(player) != 0x820C93A0u ||
      rex::memory::load_and_swap<uint8_t>(base + player + 0x16B9u) != 0)
    return original;
  const double speed = rex::cvar::Query<double>("pdz_movement_response");
  const double adjusted = classic ? pdz::ClassicMovementRetention(original, seconds, axis, old, target)
                                  : pdz::MovementRetention(original, speed);
  // Log each preset independently: a Modern launch must not suppress evidence
  // when the user switches to Classic later in the same session.
  static std::atomic<bool> reported[2][2]{};
  if (axis < 2 && adjusted != original && !reported[classic ? 1 : 0][axis].exchange(true)) {
    REXLOG_INFO("PDZ walking blend active: classic={} player={:08X} axis={} dt_s={} response={} retention={:.6f}->{:.6f}",
                classic, player, axis, seconds, speed, original, adjusted);
  }
  return adjusted;
}

namespace {
struct CoverAimState {
  uint32_t player = 0;
  uint32_t yaw = 0;
  uint32_t pitch = 0;
  int64_t updated_ms = 0;
};

struct CoverComposedState {
  uint32_t player = 0;
  float yaw = 0.0f;
  float pitch = 0.0f;
  int64_t updated_ms = 0;
  uint32_t branch = 1;  // Only branch zero uses the stable cover-relative angles.
};

std::mutex cover_aim_mutex;
CoverAimState cover_aim_state;
CoverComposedState cover_composed_state;
struct CoverTargetCamera {
  uint32_t player=0, camera=0;
  std::array<float,3> eye{};
  int64_t time=0;
  uint32_t aim_camera=0;
  std::array<float,16> display_view{};
  pdz::aim::Projection projection{};
};
CoverTargetCamera cover_target_camera;
struct CoverResolvedAim {
  uint32_t player=0,camera=0,aim_camera=0;
  std::array<float,16> aim_view{};
  pdz::aim::Projection projection{};
  pdz::aim::Point cursor{};
  double depth=0;
  int64_t time=0;
};
CoverResolvedAim cover_resolved_aim;
// Completed only after the proven local producer, ray alignment and collision.
// Never infer owner from guest r31, which is repurposed before target completion.
thread_local CoverResolvedAim cover_acquisition;
thread_local bool cover_acquisition_ray=false;
}

namespace {
std::mutex third_person_impact_mutex;
struct ThirdPersonImpactWorld {uint32_t player=0;int64_t time=0;bool blocked=false;std::array<float,3> world{};};
ThirdPersonImpactWorld third_person_impact_world;
struct ThirdPersonImpactView {uint32_t player=0;int64_t time=0;std::array<float,16> view{};pdz::aim::Projection projection{};};
ThirdPersonImpactView third_person_impact_view;
}
void PDZ_ConstrainThirdPersonTarget(PPCContext& ctx,uint8_t* base,uint32_t player,uint32_t target) {
  if(!rex::cvar::Query<bool>("pdz_tp_enable")||!GuestReadable(base,player,0x2000)||
     LocalPlayer(base)!=player||HudWord(base,player)!=0x820C93A0u||
     base[player+0x16A3]||base[player+0x16B9]||!GuestReadable(base,target,12))return;
  float eye[3],end[3];
  if(!PDZ_CopyThirdPersonClearanceEye(player,eye))return;
  {
    std::lock_guard lock(third_person_impact_mutex);
    third_person_impact_world={player,HudTime(),false,{}};
  }
  double length2=0;
  for(unsigned i=0;i<3;++i) {
    end[i]=rex::memory::load_and_swap<float>(base+target+4*i);
    if(!std::isfinite(end[i]))return;
    const double delta=double(end[i])-eye[i];length2+=delta*delta;
  }
  if(length2<1||!std::isfinite(length2)||ctx.r1.u32<0x10400)return;
  // Same native collector and layer as target acquisition. Separate guest
  // scratch and context preserve the original query's collector and registers.
  const uint32_t sp=(ctx.r1.u32-0x400)&~0xFu;
  if(!GuestReadable(base,sp,0x400))return;
  const uint32_t from=sp+0x200,to=sp+0x210,collector=sp+0x280;
  PPCContext call=ctx;call.r1.u64=sp;
  for(unsigned i=0;i<3;++i) {
    rex::memory::store_and_swap<float>(base+from+4*i,eye[i]);
    rex::memory::store_and_swap<float>(base+to+4*i,end[i]);
  }
  std::memset(base+collector,0,0x100);
  rex::memory::store_and_swap<uint32_t>(base+collector,0x820925C0u);
  const auto invoke=[&](unsigned slot) {
    const auto address=HudWord(base,0x820925C0u+slot);
    auto* fn=rex::runtime::ResolveIndirectFunction(address);
    if(!fn)return false;
    call.r3.u64=collector;fn(call,base);return true;
  };
  if(!invoke(12))return;
  call.r3.u64=32;call.r4.u64=from;call.r5.u64=to;
  call.r6.u64=collector;call.r7.u64=1;
  sub_82252798(call,base);
  if(!invoke(8)||(call.r3.u32&0xFF)==0)return;
  const float fraction=rex::memory::load_and_swap<float>(base+collector+80);
  if(!std::isfinite(fraction)||fraction<0||fraction>=1)return;
  float hit[3];
  for(unsigned i=0;i<3;++i) {
    hit[i]=rex::memory::load_and_swap<float>(base+collector+68+4*i);
    if(!std::isfinite(hit[i]))return;
  }
  for(unsigned i=0;i<3;++i)rex::memory::store_and_swap<float>(base+target+4*i,hit[i]);
  {
    std::lock_guard lock(third_person_impact_mutex);
    third_person_impact_world={player,HudTime(),true,{hit[0],hit[1],hit[2]}};
  }
}
bool PDZ_ReadThirdPersonImpact(PDZ_ThirdPersonImpact& impact) {
  impact={};
  if(!rex::cvar::Query<bool>("pdz_tp_enable")||pdz::UiMouseCaptured())return false;
  auto* runtime=rex::Runtime::instance();
  auto* base=runtime?runtime->virtual_membase():nullptr;
  const auto player=base?LocalPlayer(base):0;
  if(!GuestReadable(base,player,0x2000)||base[player+0x16A3]||base[player+0x16B9])return false;
  float eye[3];if(!PDZ_CopyThirdPersonClearanceEye(player,eye))return false;
  ThirdPersonImpactWorld world;ThirdPersonImpactView view;
  {std::lock_guard lock(third_person_impact_mutex);world=third_person_impact_world;view=third_person_impact_view;}
  const auto now=HudTime();
  if(world.player!=player||view.player!=player||now<world.time||now-world.time>100||
     now<view.time||now-view.time>100)return false;
  if(!world.blocked)return true;
  pdz::aim::Point screen;double depth=0;
  if(!pdz::CoverProject(view.view,view.projection,world.world,screen,depth)||
     std::abs(screen.x)>1||std::abs(screen.y)>1)return false;
  impact={screen.x,screen.y,true};return true;
}
void PDZ_RecordCoverTarget(uint8_t* base,uint32_t target) {
  if(!cover_acquisition_ray) return;
  cover_acquisition_ray=false;
  if(!base||!rex::cvar::Query<bool>("pdz_cover_camera_stabilization")||
     !GuestReadable(base,target,12)||LocalPlayer(base)!=cover_acquisition.player||
     !GuestReadable(base,cover_acquisition.player,0x16BA)||
     base[cover_acquisition.player+0x16B9]!=1||base[cover_acquisition.player+0x16A3]) return;
  const auto now=HudTime();
  if(now<cover_acquisition.time||now-cover_acquisition.time>100) return;
  std::array<float,3> world{};
  for(unsigned i=0;i<3;++i) world[i]=rex::memory::load_and_swap<float>(base+target+4*i);
  pdz::aim::Point projected; double depth=0;
  if(!pdz::CoverProject(cover_acquisition.aim_view,cover_acquisition.projection,
                        world,projected,depth)) return;
  std::lock_guard lock(cover_aim_mutex);
  if(cover_target_camera.player!=cover_acquisition.player||
     cover_target_camera.camera!=cover_acquisition.camera||
     cover_target_camera.aim_camera!=cover_acquisition.aim_camera||
     cover_composed_state.player!=cover_acquisition.player||
     cover_composed_state.branch!=0) return;
  cover_acquisition.depth=depth;cover_acquisition.time=now;
  cover_resolved_aim=cover_acquisition;
}

#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
void PDZ_TraceCoverAimOrder(uint8_t* base,uint32_t player,unsigned stage,
                           uint32_t first,uint32_t second,unsigned count,uint32_t detail) {
  static const bool enabled=[] { const char* p=std::getenv("PDZ_COVER_AIM_TRACE");
    return p && std::strcmp(p,"1")==0; }();
  if(!enabled || !base || !count || count>16) return;
  static thread_local uint32_t aim_player=0;
  if(stage==0) aim_player=player;
  if(!player) player=aim_player;
  if(!GuestReadable(base,player,0x16BA) || LocalPlayer(base)!=player ||
     base[player+0x16B9]!=1 || base[player+0x16A3] ||
     !rex::cvar::Query<bool>("pdz_cover_camera_stabilization")) return;
  if(!GuestReadable(base,first,count*4) ||
     (second && !GuestReadable(base,second,count*4))) return;
  static std::mutex mutex;
  static int64_t start=-1;
  static unsigned sequence=0;
  std::lock_guard lock(mutex);
  const auto now=HudTime();
  if(start<0) start=now;
  if(now<start || now-start>20000 || sequence>=3000) return;
  const auto event=++sequence;
  const auto thread=std::hash<std::thread::id>{}(std::this_thread::get_id());
  // Raw words avoid interpreting pointers/flags as floats. No guest writes.
  for(unsigned i=0;i<count;i+=4) {
    std::array<uint32_t,4> a{},b{};
    for(unsigned j=0;j<4 && i+j<count;++j) {
      a[j]=HudWord(base,first+4*(i+j));
      if(second) b[j]=HudWord(base,second+4*(i+j));
    }
    REXLOG_INFO("PDZ COVER_ORDER seq={} ms={} thread={} stage={} player={:08X} "
      "detail={:08X} first={:08X} second={:08X} count={} row={} "
      "a={:08X},{:08X},{:08X},{:08X} b={:08X},{:08X},{:08X},{:08X}",
      event,now,thread,stage,player,detail,first,second,count,i,
      a[0],a[1],a[2],a[3],b[0],b[1],b[2],b[3]);
  }
}

#endif

// Called before suppressing the original renderer. Its draw dispatch and
// visibility/alpha fields decide whether this is an armed aiming context.
extern "C" void PDZ_ObserveOriginalCrosshair(uint8_t* base, uint32_t object, bool spread) {
  const auto flags = rex::memory::load_and_swap<uint32_t>(base + object + 56);
  const auto alpha = std::bit_cast<float>(rex::memory::load_and_swap<uint32_t>(base + object + 4));
  if (!(flags & 0x20) || !(alpha > 0.0f)) return;
  if (!spread && !rex::memory::load_and_swap<uint32_t>(base + object + 12) && !rex::memory::load_and_swap<uint32_t>(base + object + 16)) return;
  double aim_x=0,aim_y=0;
  if(PDZ_ReadVisualCrosshair(aim_x,aim_y)) {
    const uint32_t root=HudWord(base,0x82E35468u);
    const uint32_t player=HudPointer(root,0x3BC)?HudWord(base,root+0x3B8):0;
    const uint32_t weapon=HudPointer(player,4084)?HudWord(base,player+4080):0;
    if(HudPointer(weapon,1280)&&HudWord(base,weapon+1276)==object) {
      rex::memory::store_and_swap<float>(base+object+20,float(aim_x));
      rex::memory::store_and_swap<float>(base+object+24,float(-aim_y));
      PDZ_TraceCoverAimOrder(base,player,20,object+20,0,2,object);
    }
  }
  crosshair_last_visible_ms.store(CrosshairClockMs(), std::memory_order_relaxed);
}

bool PDZ_ShouldDrawCustomCrosshair() {
  const auto last = crosshair_last_visible_ms.load(std::memory_order_relaxed);
  // Game rendering and overlay presentation run asynchronously. Allow a few
  // frames of latency, but expire when menus/cutscenes stop reticle drawing.
  return last != 0 && CrosshairClockMs() - last <= 120;
}

#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
void PDZ_TraceStickCall(uint32_t input_slot_address, uint32_t link_register,
                        uint32_t horizontal_address, uint32_t vertical_address) {
  // The public wrappers pass either the four left-stick slots or the four
  // right-stick slots by advancing the input array pointer by four bytes.
  // Record each caller once so we can follow the right-stick output into the
  // actual camera update and move mouse handling beyond analog sensitivity.
  static std::mutex mutex;
  static std::unordered_set<uint64_t> seen;
  const uint64_t key = (uint64_t(input_slot_address) << 32) | link_register;
  std::lock_guard<std::mutex> lock(mutex);
  if (seen.insert(key).second) {
    REXLOG_INFO(
        "PDZ stick conditioner caller: input={:08X}, lr={:08X}, horizontal={:08X}, vertical={:08X}",
        input_slot_address, link_register, horizontal_address, vertical_address);
  }
}

void PDZ_TraceCameraRead(uint32_t link_register, uint32_t input_state_address) {
  static std::mutex mutex;
  static std::unordered_set<uint32_t> seen;
  std::lock_guard<std::mutex> lock(mutex);
  if (seen.insert(link_register).second) {
    REXLOG_INFO("PDZ camera-axis reader caller: lr={:08X}, state={:08X}",
                link_register, input_state_address);
  }
}

void PDZ_TraceVisionState(uint8_t* base, uint32_t state_address) {
  if (!base || state_address < 0x10000u || state_address > UINT32_MAX - 796u) return;
  const auto load_u32 = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  const uint32_t mode = load_u32(state_address + 732);
  if (mode != 2) return;

  // This call is at the precise mode-2 composite setup. Record only state
  // changes: this is diagnostic data, not a rendering modification.
  const uint32_t source = load_u32(state_address + 736);
  const uint32_t value_740 = load_u32(state_address + 740);
  const uint32_t value_744 = load_u32(state_address + 744);
  const uint32_t value_788 = load_u32(state_address + 788);
  const uint32_t value_792 = load_u32(state_address + 792);
  const uint32_t key = state_address ^ source ^ value_740 ^ value_744 ^
                       value_788 ^ value_792;
  static std::atomic<uint32_t> last_key{0};
  if (last_key.exchange(key, std::memory_order_relaxed) == key) return;

  REXLOG_INFO(
      "PDZ RCP90 mode2 state={:08X} src={:08X} f740={:.6f} f744={:.6f} "
      "u748={} u724={} u725={} u727={} u788={:08X} f792={:.6f}",
      state_address, source, std::bit_cast<float>(value_740),
      std::bit_cast<float>(value_744), *(base + state_address + 748),
      *(base + state_address + 724), *(base + state_address + 725),
      *(base + state_address + 727), value_788,
      std::bit_cast<float>(value_792));
}

void PDZ_TraceRcpSubmission(uint8_t* base, uint32_t descriptor, uint32_t stage) {
  if (!base || descriptor > UINT32_MAX - 800 ||
      rex::memory::load_and_swap<uint32_t>(base + 0x82E3B93Cu) == 0) return;
  const auto u32 = [base, descriptor](uint32_t offset) {
    return rex::memory::load_and_swap<uint32_t>(base + descriptor + offset);
  };
  static std::atomic<uint32_t> last{0};
  const uint32_t key = descriptor ^ stage ^ u32(656) ^ u32(796);
  if (last.exchange(key, std::memory_order_relaxed) == key) return;
  REXLOG_INFO("PDZ RCP90 submit desc={:08X} stage={} a656={:08X} a796={:08X} f208={:.5f} f212={:.5f} f216={:.5f} f220={:.5f}", descriptor, stage, u32(656), u32(796), std::bit_cast<float>(u32(208)), std::bit_cast<float>(u32(212)), std::bit_cast<float>(u32(216)), std::bit_cast<float>(u32(220)));
}

#endif
namespace {
std::atomic<int64_t> locktopus_mouse_until{0};
}

bool PDZ_LocktopusInputActive() {
  return HudTime()<locktopus_mouse_until.load(std::memory_order_relaxed);
}

void PDZ_ApplyLocktopusMouse(uint8_t* base,uint32_t puzzle,double& x,double& y,double seconds) {
  static std::mutex mutex;
  static pdz::LocktopusMouse mouse;
  std::lock_guard lock(mutex);
  double total_x=0,total_y=0; uint64_t epoch=0;
  const auto now=HudTime();
  const bool captured=rex::input::mnk::ReadCapturedMouseTotals(total_x,total_y,epoch);
  const auto player=base?LocalPlayer(base):0;
  bool eligible=base&&player&&GuestReadable(base,puzzle,160)&&
      HudWord(base,puzzle)==0x820CEBE0u&&HudWord(base,puzzle+32)==1&&
      !pdz::UiMouseCaptured()&&std::isfinite(seconds)&&seconds>0&&seconds<=.25;
  if(eligible) {
    const auto item=HudWord(base,puzzle+4);
    eligible=GuestReadable(base,item,96)&&HudWord(base,item+92)==player;
  }
  // +16A3 is deliberately NOT a pause guard here: the live active puzzle sets it.
  // Native input consumption with advancing simulation time owns the short lease.
  locktopus_mouse_until.store(eligible?now+100:0,std::memory_order_relaxed);
  const bool mouse_enabled=eligible&&captured&&rex::cvar::Query<bool>("pdz_direct_mouse");
  [[maybe_unused]] const bool applied=mouse.Update(puzzle,epoch,now,mouse_enabled,total_x,total_y,x,y);
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  static std::atomic<bool> reported_active{false},reported_applied{false};
  if(eligible&&!reported_active.exchange(true))
    REXLOG_INFO("PDZ Locktopus native input active: puzzle={:08X} player={:08X} playerFlag={} captured={} seconds={:.6f}",puzzle,player,base[player+0x16A3],captured,seconds);
  if(applied&&!reported_applied.exchange(true))
    REXLOG_INFO("PDZ Locktopus mouse probe applied: puzzle={:08X} x={:.5f} y={:.5f}",puzzle,x,y);
#endif
}

void PDZ_ApplyDirectMouse(uint8_t* base, uint32_t horizontal_address,
                          uint32_t vertical_address, uint32_t input_slot) {
  const bool direct_mouse=rex::cvar::Query<bool>("pdz_direct_mouse");
  const bool native_gyro=rex::cvar::Query<std::string>("pdz_gyro_mode")!="off";
  const bool modern_pad=rex::cvar::Query<std::string>("pdz_stick_curve")!="original"||native_gyro;
  if (!direct_mouse&&!modern_pad&&!native_gyro) return;

  // PDZ calls the shared conditioner with slots 0-3 for the left stick and
  // 4-7 for the right stick. Consuming relative motion on the first pass
  // routes it into character movement and leaves none for camera input.
  if (input_slot != 4) return;

  float dx = 0.0f;
  float dy = 0.0f;
  bool has_motion = direct_mouse&&rex::input::mnk::ConsumeRelativeMouseMotion(&dx, &dy);
  if(HudTime()<locktopus_mouse_until.load(std::memory_order_relaxed)) {
    dx=dy=0; has_motion=false;
  }
  if (pdz::UiMouseCaptured()) {
    ResetFreeAim();
    // This native path bypasses the SDK's synthetic-stick focus callback.
    // Drain UI movement without changing any player/camera state.
    *reinterpret_cast<rex::be<uint32_t>*>(base + horizontal_address) = 0;
    *reinterpret_cast<rex::be<uint32_t>*>(base + vertical_address) = 0;
    return;
  }

  auto valid_guest_address = [base](uint32_t address, size_t bytes = 4) {
    if (address < 0x00010000u || bytes == 0 ||
        address > UINT32_MAX - bytes) {
      return false;
    }
    size_t length = bytes;
    rex::memory::PageAccess access = rex::memory::PageAccess::kNoAccess;
    return rex::memory::QueryProtect(base + address, length, access) &&
           access != rex::memory::PageAccess::kNoAccess;
  };
  auto load_u32 = [base](uint32_t address) {
    return uint32_t(*reinterpret_cast<rex::be<uint32_t>*>(base + address));
  };
  auto store_u32 = [base](uint32_t address, uint32_t value) {
    *reinterpret_cast<rex::be<uint32_t>*>(base + address) = value;
  };
  auto load_f32 = [&load_u32](uint32_t address) {
    PPCRegister value{};
    value.u32 = load_u32(address);
    return value.f32;
  };

  // Ported from Xenia Canary MouseHook's Perfect Dark Zero TU0 hook. Prefer
  // the local-player pointer (works in co-op), then fall back to Joanna's
  // mission/combat-arena pointer.
  uint32_t player = 0;
  const uint32_t local_player_root = load_u32(0x82E35468u);
  if (valid_guest_address(local_player_root)) {
    player = load_u32(local_player_root + 0x3B8u);
  }
  if (!valid_guest_address(player)) {
    player = load_u32(0x82D2AD38u);
  }

  // Loading screens replace these objects. Require the entire chain to stay
  // unchanged for several input polls before writing through it, so a stale
  // pointer from the outgoing level can't corrupt the incoming level.
  const uint32_t candidate_yaw_root =
      valid_guest_address(player + 0xE4u) ? load_u32(player + 0xE4u) : 0;

  // PDZ's universal camera selector is reached through the FOV manager. The
  // pointed-to objects commonly live in the 0x40xxxxxx range, so validate the
  // mapped host page instead of assuming that only low/0x8 guest addresses are
  // usable.
  const uint32_t fov_root = valid_guest_address(0x82D68320u)
                                ? load_u32(0x82D68320u)
                                : 0;
  const uint32_t camera_mode_root = valid_guest_address(fov_root + 0x53Cu)
                                        ? load_u32(fov_root + 0x53Cu)
                                        : 0;
  const uint8_t camera_mode = valid_guest_address(camera_mode_root + 0xBu, 1)
                                  ? *(base + camera_mode_root + 0xBu)
                                  : 0xFFu;
  const bool cover_flag = valid_guest_address(player + 0x16B9u, 1) &&
                          *(base + player + 0x16B9u) == 1;
  const bool turret_flag = valid_guest_address(player + 0x16ABu, 1) &&
                           *(base + player + 0x16ABu) == 1;
  // The per-player special-camera flags can remain set after their camera is
  // no longer active (observed live: hovercraft mode 2 with turret_flag=1).
  // Prefer the universal selector whenever its pointer chain is available and
  // use the old flags only as a fallback while that chain is absent.
  const bool has_camera_mode = camera_mode != 0xFFu;
  const bool in_cover = camera_mode == 3 || (!has_camera_mode && cover_flag);
  const bool in_turret = camera_mode == 6 || (!has_camera_mode && turret_flag);
  const bool in_vehicle_camera = camera_mode == 2 || camera_mode == 4;

  // CamSpy is not a vehicle-camera mode. Its own actor owns degree-valued
  // yaw/pitch; Joanna is marked inactive while piloting it, which previously
  // sent the mouse through the slow synthetic-controller fallback.
  const uint32_t camspy = valid_guest_address(player + 0x1ECCu)
                              ? load_u32(player + 0x1ECCu) : 0;
  const bool in_camspy = valid_guest_address(player) &&
      valid_guest_address(camspy, 1572) &&
      pdz::CamSpyControlled(load_u32(player), load_u32(camspy), player,
                           load_u32(camspy + 1536u), *(base + camspy + 1571u)) &&
      valid_guest_address(load_u32(camspy + 1560u));
  if (in_camspy) {
    static std::atomic<bool> reported{false};
    if (!reported.exchange(true))
      REXLOG_INFO("PDZ CamSpy direct mouse route active: player={:08X} camspy={:08X} yaw={:08X} pitch={:08X}",
                  player, camspy, camspy + 1380u, camspy + 1376u);
  }

  {
    std::lock_guard<std::mutex> lock(cover_aim_mutex);
    const bool paused = valid_guest_address(player + 0x16A3u, 1) &&
                        *(base + player + 0x16A3u) != 0;
    if (!in_cover || !cover_flag || paused || cover_aim_state.player != player) {
      cover_aim_state = {};
    } else if (!has_motion && cover_aim_state.updated_ms) {
      // Keep the chosen look direction when the mouse stops; the composer
      // still runs during the remainder of the cover animation.
      cover_aim_state.updated_ms = CrosshairClockMs();
    }
  }
  const bool free_aim_enabled=rex::cvar::Query<bool>("pdz_classic_free_aim");
  const bool aiming=AimHeld(base,player);
  const bool free_gesture=pdz::aim::FreeAimActive(free_aim_enabled,
      rex::cvar::Query<bool>("pdz_free_aim_ads_only"),aiming) &&
      pdz::aim::ManualSwayActive(rex::cvar::Query<bool>("pdz_sway_during_ads"),aiming) &&
      pdz::aim::FreeAimCamera(camera_mode,aiming) && !in_camspy && valid_guest_address(player+0x16A3u,1) &&
      !base[player+0x16A3u];
  if(!free_gesture) ResetFreeAim();
  if (!has_motion && !free_gesture && !modern_pad && !native_gyro) return;

  uint32_t yaw_address = candidate_yaw_root + 0x150u;
  uint32_t pitch_address = player + 0x1674u;
  bool yaw_is_degrees = false;
  bool pitch_is_radians = false;
  uint32_t special_camera_root = 0;
  if (in_camspy) {
    yaw_address = camspy + 1380u;
    pitch_address = camspy + 1376u;
    yaw_is_degrees = true;
  } else if (in_cover) {
    yaw_address = player + 0x1670u;
    yaw_is_degrees = true;
  } else if (in_turret || in_vehicle_camera) {
    special_camera_root = valid_guest_address(player + 0x5Cu)
                              ? load_u32(player + 0x5Cu)
                              : 0;
    if (in_turret) {
      yaw_address = special_camera_root + 0x3A0u;
      pitch_address = special_camera_root + 0x39Cu;
    } else {
      yaw_address = special_camera_root + 0xE58u;
      pitch_address = special_camera_root + 0xE54u;
    }
    pitch_is_radians = true;
  }

  static uint32_t last_player = 0;
  static uint32_t last_yaw_root = 0;
  static uint32_t last_yaw_address = 0;
  static uint32_t last_pitch_address = 0;
  static uint32_t stable_polls = 0;
  if (player != last_player || candidate_yaw_root != last_yaw_root ||
      yaw_address != last_yaw_address || pitch_address != last_pitch_address) {
    last_player = player;
    last_yaw_root = candidate_yaw_root;
    last_yaw_address = yaw_address;
    last_pitch_address = pitch_address;
    stable_polls = 0;
  } else if (stable_polls < 8) {
    ++stable_polls;
  }

  // In gameplay, write raw mouse motion directly to the camera state and
  // suppress the synthetic right stick. This bypasses controller sensitivity,
  // deadzones, acceleration, smoothing and maximum turn speed entirely.
  if (valid_guest_address(player)) {
    const uint32_t yaw_root = candidate_yaw_root;
    const bool paused = valid_guest_address(player + 0x16A3u, 1) &&
                        *(base + player + 0x16A3u) != 0;
    const bool valid_camera = valid_guest_address(yaw_root) &&
                              valid_guest_address(yaw_address) &&
                              valid_guest_address(pitch_address);
    if ((!paused || camera_mode == 4 || in_camspy) && valid_camera && stable_polls < 3) {
      store_u32(horizontal_address, 0);
      store_u32(vertical_address, 0);
      return;
    }
    if ((!paused || camera_mode == 4 || in_camspy) && stable_polls >= 3 && valid_camera) {
      PPCRegister value{};

      value.u32 = load_u32(yaw_address);
      float yaw_value = value.f32;
      value.u32 = load_u32(pitch_address);
      float pitch_degrees = pitch_is_radians
                                ? value.f32 * (180.0f / 3.14159265358979323846f)
                                : value.f32;

      // The cover update owns these same fields and can restore its animation
      // baseline between input polls. Continue from the last requested cover
      // aim instead of that transient value, but never carry it across a pause,
      // mode change, or player replacement.
      if (in_cover) {
        std::lock_guard<std::mutex> lock(cover_aim_mutex);
        const auto age = CrosshairClockMs() - cover_aim_state.updated_ms;
        if (cover_aim_state.player == player && age >= 0 && age <= 100) {
          yaw_value = std::bit_cast<float>(cover_aim_state.yaw);
          pitch_degrees = std::bit_cast<float>(cover_aim_state.pitch);
        }
      }
      // Reject transitional/uninitialized camera data even when its pointer
      // happens to fall inside the guest arena.
      if (!std::isfinite(yaw_value) || !std::isfinite(pitch_degrees) ||
          std::abs(yaw_value) > 10000.0f ||
          std::abs(pitch_degrees) > 180.0f) {
        stable_polls = 0;
        store_u32(horizontal_address, 0);
        store_u32(vertical_address, 0);
        return;
      }

      constexpr float kPi = 3.14159265358979323846f;
      constexpr float kMouseCountsPerDegree = 8.40517241378f;
      const float sensitivity =
          float(rex::cvar::Query<double>("pdz_mouse_sensitivity"));

      // The live value at +0x440 follows weapon zoom. Port MouseHook's
      // magnification compensation, blended with 1.0 so users can reduce or
      // disable it without changing their hip-fire sensitivity.
      float zoom_factor = 1.0f;
      bool is_zoomed = false;
      float traced_base_fov = 0.0f;
      float traced_live_fov = 0.0f;
      const uint8_t traced_camera_mode = camera_mode;
      if (!in_camspy && valid_guest_address(fov_root + 0x440u)) {
        const float base_fov = load_f32(0x820EC228u);
        const float live_fov = load_f32(fov_root + 0x440u);
        traced_base_fov = base_fov;
        traced_live_fov = live_fov;
        if (std::isfinite(base_fov) && std::isfinite(live_fov) &&
            base_fov > 1.0f && live_fov > 0.01f) {
          float magnification = base_fov / live_fov;
          if (magnification >= 45.0f) magnification /= 2.5f;
          if (magnification > 1.0f) {
            is_zoomed = true;
            const float amount = float(rex::cvar::Query<double>(
                "pdz_mouse_zoom_compensation"));
            zoom_factor = 1.0f + (magnification - 1.0f) * amount;
          }
        }
      }
      // Some weapons expose their aiming state before the live FOV value has
      // settled. Treat that flag as zoomed too, so the relative zoom slider is
      // effective throughout the transition into and out of sights.
      if (!in_camspy && valid_guest_address(player + 0x1910u)) {
        const float gun_zoom = load_f32(player + 0x1910u);
        if (std::isfinite(gun_zoom) && gun_zoom > 0.0f && gun_zoom < 1000.0f) {
          is_zoomed = true;
        }
      }
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
      if (rex::cvar::Query<bool>("pdz_trace_camera_state")) {
        static uint64_t previous_state = UINT64_MAX;
        const uint64_t state = uint64_t(player) ^ (uint64_t(fov_root) << 7) ^
                               (uint64_t(camera_mode_root) << 19) ^
                               (uint64_t(traced_camera_mode) << 51) ^
                               (uint64_t(std::bit_cast<uint32_t>(traced_live_fov)) << 3);
        if (state != previous_state) {
          previous_state = state;
          REXLOG_INFO(
              "PDZ camera state: player={:08X}, fov_root={:08X}, mode_root={:08X}, mode={}, cover={}, turret={}, special_root={:08X}, yaw={:08X}, pitch={:08X}, base_fov={:.5f}, live_fov={:.5f}, zoomed={}",
              player, fov_root, camera_mode_root, traced_camera_mode, in_cover,
              in_turret, special_camera_root, yaw_address, pitch_address,
              traced_base_fov, traced_live_fov, is_zoomed);
          rex::FlushLogging();
        }
      }
#endif
      const float zoom_sensitivity =
          is_zoomed ? float(rex::cvar::Query<double>(
                          "pdz_mouse_zoom_sensitivity_scale"))
                    : 1.0f;
      float degrees_x = dx * sensitivity * zoom_sensitivity /
                              (kMouseCountsPerDegree * zoom_factor);
      float degrees_y = dy * sensitivity * zoom_sensitivity /
                              (kMouseCountsPerDegree * zoom_factor);
      const auto gyro_turn=PDZ_GyroTurn(aiming);
      const double mouse_demand=std::hypot(degrees_x,degrees_y);
      const double gyro_demand=std::hypot(gyro_turn.x/zoom_factor,gyro_turn.y/zoom_factor);
      degrees_x+=float(gyro_turn.x/zoom_factor);
      degrees_y+=float(gyro_turn.y/zoom_factor);
      const auto pad_turn=PDZ_ControllerTurn(aiming);
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
      {
        std::lock_guard lock(aim_input_trace_mutex);
        aim_input_trace={player,gyro_turn.x/zoom_factor,gyro_turn.y/zoom_factor,
                         pad_turn.x,pad_turn.y,std::chrono::steady_clock::now()};
      }
#endif

      if(free_gesture && !paused) {
        const uint32_t controller=valid_guest_address(player+6560u)?load_u32(player+6560u):0;
        const uint32_t aim_camera=valid_guest_address(controller+1148u)?load_u32(controller+1148u):0;
        pdz::aim::Projection projection;
        if(valid_guest_address(aim_camera,104) && ReadAimProjection(base,aim_camera,projection)) {
          const uint32_t weapon=load_u32(player+4080u);
          const auto now=HudTime();
          std::lock_guard lock(free_aim_mutex);
          if(free_aim_player!=player||free_aim_weapon!=weapon||free_aim_camera_mode!=camera_mode||now-free_aim_ms>250)
            {free_aim_state.Reset();free_aim_ms=0;free_aim_gyro_source=false;}
          if(mouse_demand>0||gyro_demand>0)free_aim_gyro_source=gyro_demand>mouse_demand;
          const double seconds=free_aim_ms?std::max(0.,(now-free_aim_ms)/1000.):.016;
          pdz::aim::Point camera_turn;
          const pdz::aim::Point motion{
            degrees_x*(kPi/180.)/(projection.tan_half_vertical*projection.aspect),
            -degrees_y*(kPi/180.)/projection.tan_half_vertical};
          if(free_aim_state.Update(motion,rex::cvar::Query<double>("pdz_crosshair_edge_deadzone"),seconds,camera_turn)) {
            free_aim_player=player;free_aim_weapon=weapon;free_aim_ms=now;free_aim_camera_mode=camera_mode;
            // Edge demand replaces input velocity; apply zoom/ADS here too.
            // Do not use the legacy >=45x compensation exception for this rate.
            const double ads=aiming?(free_aim_gyro_source?
                rex::cvar::Query<double>("pdz_gyro_ads"):double(zoom_sensitivity)):1.;
            const double edge_scale=pdz::aim::EdgeZoomScale(traced_base_fov,traced_live_fov,ads);
            degrees_x=float(camera_turn.x*90.*edge_scale);
            degrees_y=float(-camera_turn.y*90.*edge_scale);
          }
        } else ResetFreeAim();
      } else ResetFreeAim();

      // Stick turns the camera; only mouse/gyro participate in free aim.
      degrees_x+=float(pad_turn.x);degrees_y+=float(pad_turn.y);
      if(!in_cover&&!in_camspy&&std::isfinite(degrees_x)&&std::isfinite(degrees_y)) {
        std::lock_guard lock(aim_input_trace_mutex);
        const auto now=std::chrono::steady_clock::now();
        if(sway_input.player!=player)
          sway_input={player,0,0,now};
        // Record intentional camera demand before recoil/assist transforms.
        pdz::AccumulateSwayInput(degrees_x,degrees_y,sway_input.yaw,sway_input.pitch);
        sway_input.time=now;
      }
      if (yaw_is_degrees) {
        // CamSpy turns freely; only cover uses the +/-68 degree constraint.
        yaw_value = in_camspy ? pdz::CamSpyYaw(yaw_value, degrees_x)
                             : std::clamp(yaw_value - degrees_x, -68.0f, 68.0f);
      } else {
        yaw_value -= degrees_x * (kPi / 180.0f);
      }
      pitch_degrees += degrees_y;
      pitch_degrees = std::clamp(pitch_degrees, -90.0f, 90.0f);

      value.f32 = yaw_value;
      store_u32(yaw_address, value.u32);
      value.f32 = pitch_is_radians ? pitch_degrees * (kPi / 180.0f)
                                   : pitch_degrees;
      store_u32(pitch_address, value.u32);
      if (in_cover) {
        std::lock_guard<std::mutex> lock(cover_aim_mutex);
        cover_aim_state = {player, load_u32(yaw_address),
                           load_u32(pitch_address), CrosshairClockMs()};
      }
      store_u32(horizontal_address, 0);
      store_u32(vertical_address, 0);
      return;
    }
  }

  // Menus and unsupported camera modes still need a momentary right-stick
  // fallback so their original navigation remains usable.
  if(!has_motion) return;
  constexpr float kCountsPerPixel = 1450.0f;
  float sensitivity = float(rex::cvar::Query<double>("pdz_mouse_sensitivity"));
  auto axis = [sensitivity](float pixels) {
    return int32_t(std::clamp(std::lround(pixels * sensitivity * kCountsPerPixel),
                              long(-32767), long(32767)));
  };
  *reinterpret_cast<rex::be<uint32_t>*>(base + horizontal_address) = uint32_t(axis(dx));
  *reinterpret_cast<rex::be<uint32_t>*>(base + vertical_address) = uint32_t(axis(-dy));
}

void PDZ_TraceCoverCamera(uint8_t* base, uint32_t player, uint32_t camera,
                          uint32_t branch_state, uint32_t phase) {
  struct Snapshot {
    uint32_t player = 0;
    uint32_t camera = 0;
    uint32_t branch = 0;
    int64_t captured_ms = 0;
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
    uint32_t requested_yaw = 0;
    uint32_t requested_pitch = 0;
    int64_t request_age_ms = -1;
    float pre_yaw = 0.0f;
    float pre_pitch = 0.0f;
    float zoom = 0.0f;
    float transition_a = 0.0f;
    float transition_b = 0.0f;
    float view_angle_a = 0.0f;
    float view_angle_b = 0.0f;
    float view_degrees_a = 0.0f;
    float view_degrees_b = 0.0f;
    uint8_t cover = 0;
    bool should_log = false;
#endif
  };
  static std::mutex mutex;
  static Snapshot snapshot;
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  static int64_t last_capture_ms = 0;
#endif

  if (!base || player < 0x10000u || camera < 0x10000u) return;
  const auto load_u32 = [base](uint32_t address) {
    return rex::memory::load_and_swap<uint32_t>(base + address);
  };
  const auto load_f32 = [&load_u32](uint32_t address) {
    return std::bit_cast<float>(load_u32(address));
  };
  const int64_t now = CrosshairClockMs();
  std::lock_guard<std::mutex> lock(mutex);
  if (phase == 0) {
    snapshot = {};
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
    snapshot.should_log = now - last_capture_ms >= 25;
    if (snapshot.should_log) last_capture_ms = now;
#endif
    snapshot.player = player;
    snapshot.camera = camera;
    snapshot.branch = branch_state;
    snapshot.captured_ms = now;
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
    snapshot.pre_yaw = load_f32(player + 0x1670u);
    snapshot.pre_pitch = load_f32(player + 0x1674u);
    snapshot.zoom = load_f32(player + 0x1910u);
    snapshot.transition_a = load_f32(player + 7328u);
    snapshot.transition_b = load_f32(player + 7392u);
    snapshot.view_angle_a = load_f32(player + 0x1924u);
    snapshot.view_angle_b = load_f32(player + 0x192Cu);
    snapshot.view_degrees_a = load_f32(player + 0x18E8u);
    snapshot.view_degrees_b = load_f32(player + 0x18FCu);
    snapshot.cover = *(base + player + 0x16B9u);
#endif
    {
      std::lock_guard<std::mutex> aim_lock(cover_aim_mutex);
      if (branch_state != 0) {
        // Invalidate before composing, not just afterwards: a view upload
        // must not reuse the previous steady-cover correction during exit.
        cover_composed_state = {player, 0.0f, 0.0f, now, branch_state};
      }
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
      if (cover_aim_state.player == player) {
        snapshot.requested_yaw = cover_aim_state.yaw;
        snapshot.requested_pitch = cover_aim_state.pitch;
        snapshot.request_age_ms = now - cover_aim_state.updated_ms;
      }
#endif
    }
    return;
  }
  if (snapshot.player != player || snapshot.camera != camera ||
      now - snapshot.captured_ms > 100) return;
  const float post_yaw = load_f32(player + 0x1670u);
  const float post_pitch = load_f32(player + 0x1674u);
  {
    std::lock_guard<std::mutex> aim_lock(cover_aim_mutex);
    cover_composed_state = {player, post_yaw, post_pitch, now, snapshot.branch};
  }
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  if (!snapshot.should_log) {
    snapshot.player = 0;
    return;
  }
  REXLOG_INFO(
      "PDZ cover final: player={:08X} camera={:08X} branch={} cover={} "
      "zoom={:.4f} timerA={:.4f} timerB={:.4f} req_age={} "
      "req_yaw={:.4f} req_pitch={:.4f} pre_yaw={:.4f} pre_pitch={:.4f} "
      "post_yaw={:.4f} post_pitch={:.4f} view_rad=({:.5f},{:.5f}) "
      "view_deg=({:.4f},{:.4f}) cam320={:.4f} cam428={:.4f} cam456={:.4f}",
      player, camera, snapshot.branch, snapshot.cover, snapshot.zoom,
      snapshot.transition_a, snapshot.transition_b, snapshot.request_age_ms,
      std::bit_cast<float>(snapshot.requested_yaw),
      std::bit_cast<float>(snapshot.requested_pitch), snapshot.pre_yaw,
      snapshot.pre_pitch, post_yaw, post_pitch, snapshot.view_angle_a,
      snapshot.view_angle_b, snapshot.view_degrees_a,
      snapshot.view_degrees_b, load_f32(camera + 320u),
      load_f32(camera + 428u), load_f32(camera + 456u));
#endif
  snapshot.player = 0;
}

void PDZ_AdjustFinalCoverView(uint8_t* base, uint32_t matrix_address,
                              uint32_t caller) {
  // Called once per newly composed view, before the shared upload computes
  // both the view and its inverse. Submission branches are too late and are
  // bypassed by the alternate rendering path.
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  static std::atomic<uint32_t> entry_count{0};
  static std::atomic<int64_t> last_log_6c_ms{0};
  static std::atomic<int64_t> last_log_b8_ms{0};
  const uint32_t entry = entry_count.fetch_add(1, std::memory_order_relaxed) + 1;
#endif
  const int64_t now = CrosshairClockMs();
  const uint32_t camera_root = matrix_address >= 0x400u
                                   ? matrix_address - 0x400u
                                   : 0;
  uint32_t player = 0;
  uint8_t cover = 0;
  const auto mapped = [base](uint32_t address, size_t bytes) {
    if (!base || address < 0x10000u || address > UINT32_MAX - bytes) return false;
    size_t length = bytes;
    rex::memory::PageAccess access = rex::memory::PageAccess::kNoAccess;
    return rex::memory::QueryProtect(base + address, length, access) &&
           access != rex::memory::PageAccess::kNoAccess;
  };
  if (mapped(camera_root + 8u, 4)) {
    player = rex::memory::load_and_swap<uint32_t>(base + camera_root + 8u);
    if (mapped(player + 0x16B9u, 1)) cover = *(base + player + 0x16B9u);
  }

  CoverAimState requested;
  CoverComposedState composed;
  {
    std::lock_guard<std::mutex> lock(cover_aim_mutex);
    requested = cover_aim_state;
    composed = cover_composed_state;
  }
  const int64_t requested_age = requested.updated_ms ? now - requested.updated_ms : -1;
  const int64_t composed_age = composed.updated_ms ? now - composed.updated_ms : -1;
  const float requested_yaw = std::bit_cast<float>(requested.yaw);
  const float requested_pitch = std::bit_cast<float>(requested.pitch);
  const float yaw_delta = requested_yaw - composed.yaw;
  const float pitch_delta = requested_pitch - composed.pitch;

  const char* reason = "READY";
  if (!mapped(matrix_address, 64) || camera_root < 0x10000u) {
    reason = "INVALID_MATRIX";
  } else if (!mapped(0x82D68320u, 4) ||
             rex::memory::load_and_swap<uint32_t>(base + 0x82D68320u) != camera_root) {
    reason = "NOT_LOCAL_CAMERA";
  } else if (player < 0x10000u) {
    reason = "INVALID_ROOT_PLAYER";
  } else if (cover != 1) {
    reason = "NOT_IN_COVER";
  } else if (composed.player == player &&
             !pdz::CanCorrectCoverView(cover, composed.branch)) {
    // The cover flag lingers during exit, but the native composer has already
    // changed the yaw reference by 180 degrees. Leave that transition native.
    reason = "NATIVE_COVER_TRANSITION";
  } else if (requested.player == 0) {
    reason = "NO_REQUESTED_STATE";
  } else if (requested.player != player) {
    reason = "REQUESTED_PLAYER_MISMATCH";
  } else if (requested_age < 0 || requested_age > 100) {
    reason = "REQUESTED_STALE";
  } else if (composed.player == 0) {
    reason = "NO_COMPOSED_STATE";
  } else if (composed.player != player) {
    reason = "COMPOSED_PLAYER_MISMATCH";
  } else if (composed_age < 0 || composed_age > 100) {
    reason = "COMPOSED_STALE";
  } else if (!std::isfinite(requested_yaw) ||
             !std::isfinite(requested_pitch) ||
             !std::isfinite(composed.yaw) || !std::isfinite(composed.pitch)) {
    reason = "NONFINITE_ANGLES";
  } else if (std::abs(yaw_delta) < 0.0001f &&
             std::abs(pitch_delta) < 0.0001f) {
    reason = "ZERO_DELTA";
  }

  if (reason == std::string_view("READY")) {
    std::array<float, 16> matrix{};
    for (uint32_t i = 0; i < 16; ++i)
      matrix[i] = std::bit_cast<float>(rex::memory::load_and_swap<uint32_t>(
          base + matrix_address + i * 4));
    if (pdz::RotateCoverView(matrix, yaw_delta, pitch_delta)) {
      for (uint32_t i = 0; i < 16; ++i)
        *reinterpret_cast<rex::be<uint32_t>*>(base + matrix_address + i * 4) =
            std::bit_cast<uint32_t>(matrix[i]);
      reason = "APPLIED";
    } else {
      reason = "INVALID_VIEW_BASIS";
    }
  }

  // Adjust before sub_82643740 derives both shared camera matrices. Changing
  // only a draw-time view would leave screen-to-world projection inconsistent.
  // This does not relocate the character, muzzle, or collision geometry.
  static thread_local pdz::CoverCameraAnchor cover_anchor;
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  static thread_local int64_t anchor_log_ms = 0;
#endif
  const bool local_view = mapped(0x82D68320u, 4) && camera_root >= 0x10000u &&
      rex::memory::load_and_swap<uint32_t>(base + 0x82D68320u) == camera_root;
  if (local_view) {
    const auto word = [base](uint32_t address) {
      return rex::memory::load_and_swap<uint32_t>(base + address);
    };
    if(rex::cvar::Query<bool>("pdz_tp_enable")&&mapped(matrix_address,64)&&mapped(camera_root+60u,4)) {
      ThirdPersonImpactView snapshot; snapshot.player=player;snapshot.time=HudTime();
      for(unsigned i=0;i<16;++i)snapshot.view[i]=std::bit_cast<float>(word(matrix_address+4*i));
      if(ReadAimProjection(base,word(camera_root+60u),snapshot.projection)&&pdz::ValidCoverView(snapshot.view)) {
        std::lock_guard lock(third_person_impact_mutex);third_person_impact_view=snapshot;
      }
    }
    const bool enabled = rex::cvar::Query<bool>("pdz_cover_camera_stabilization");
    const uint32_t mode = mapped(camera_root + 0x53Cu, 4)
                              ? word(camera_root + 0x53Cu) : 0;
    const uint32_t body = mapped(player + 228u, 4) ? word(player + 228u) : 0;
    const uint32_t config = body && body < UINT32_MAX - 396u ? body + 16u : 0;
    const uint32_t attachment = mapped(player + 7344u, 4) ? word(player + 7344u) : 0;
    const bool eligible = enabled && cover == 1 && composed.player == player &&
        mapped(player, 4) && word(player) == 0x820C93A0u &&
        mapped(body, 4) && word(body) == 0x820CC6B0u &&
        composed.branch == 0 && composed_age >= 0 && composed_age <= 100 &&
        mapped(player + 0x16A3u, 1) && !base[player + 0x16A3u] &&
        mapped(mode, 12) && base[mode + 11u] == 3 &&
        mapped(config + 360u, 12) && config && mapped(matrix_address, 64) &&
        !rex::cvar::Query<bool>("pdz_tp_enable");
    if (!eligible) {
      cover_anchor.Reset();
      std::lock_guard lock(cover_aim_mutex);
      cover_target_camera={};
      cover_resolved_aim={};
    } else {
      std::array<float, 16> view{};
      std::array<float, 3> body_position{};
      for (uint32_t i = 0; i < 16; ++i)
        view[i] = std::bit_cast<float>(word(matrix_address + 4u*i));
      for (uint32_t i = 0; i < 3; ++i)
        body_position[i] = std::bit_cast<float>(word(config + 360u + 4u*i));
      const auto raw_eye = pdz::CoverViewEye(view);
      PDZ_TraceCoverAimOrder(base,player,10,matrix_address,0,16,camera_root);
      const uint32_t aim_camera=mapped(camera_root+60u,4)?word(camera_root+60u):0;
      pdz::aim::Projection display_projection;
      const bool projection_valid=ReadAimProjection(base,aim_camera,display_projection);
      bool follow_ready=false;
      CoverResolvedAim follow_target;
      {
        std::lock_guard lock(cover_aim_mutex);
        follow_target=cover_resolved_aim;
        follow_ready=projection_valid && cover_resolved_aim.player==player &&
            cover_resolved_aim.camera==camera_root &&
            cover_resolved_aim.aim_camera==aim_camera &&
            now>=cover_resolved_aim.time && now-cover_resolved_aim.time<=100 &&
            AimHeld(base,player) &&
            (rex::cvar::Query<bool>("pdz_auto_aim")||rex::cvar::Query<bool>("pdz_classic_free_aim")||
             (rex::cvar::Query<bool>("pdz_classic_weapon_sway")&&rex::cvar::Query<bool>("pdz_classic_sway_aim")));
      }
      const bool anchored = cover_anchor.Apply(view, player, camera_root,
          attachment, body_position, now, true,follow_ready?.35f:0.f);
      if(anchored && cover_anchor.follow_weight>0) {
        auto probe=follow_target.cursor;
        if(!follow_ready || !pdz::ReprojectCoverCursor(follow_target.aim_view,
            follow_target.projection,follow_target.depth,view,display_projection,probe)) {
          // A near target must not pass behind the displayed camera. Unknown or
          // stale target data restores the accepted full anchor, not guessed depth.
          pdz::SetCoverViewEye(view,{cover_anchor.anchor[0],raw_eye[1],cover_anchor.anchor[2]});
          cover_anchor.follow_weight=0;
        }
      }
      if (anchored)
        for (uint32_t i = 0; i < 16; ++i)
          *reinterpret_cast<rex::be<uint32_t>*>(base + matrix_address + 4u*i) =
              std::bit_cast<uint32_t>(view[i]);
      {
        std::lock_guard lock(cover_aim_mutex);
        // The virtual aiming eye stays anchored. Only the displayed eye follows.
        const std::array<float,3> aim_eye{cover_anchor.anchor[0],raw_eye[1],cover_anchor.anchor[2]};
        cover_target_camera=projection_valid && cover_anchor.player==player && now-cover_anchor.start_ms>=200
            ? CoverTargetCamera{player,camera_root,aim_eye,now,aim_camera,view,display_projection}
            : CoverTargetCamera{};
        if(!cover_target_camera.player) cover_resolved_aim={};
      }
      PDZ_TraceCoverAimOrder(base,player,11,matrix_address,0,16,camera_root);
      PDZ_TraceCoverAimOrder(base,player,12,camera_root+112,camera_root+176,16,camera_root);
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
      if (now - anchor_log_ms >= 100) {
        anchor_log_ms = now;
        const auto final_eye = pdz::CoverViewEye(view);
        REXLOG_INFO("PDZ cover camera anchor: player={:08X} attachment={:08X} applied={} "
                    "native_eye=({:.3f},{:.3f},{:.3f}) final_eye=({:.3f},{:.3f},{:.3f}) "
                    "horizontal_shift={:.3f} native_units",
                    player, attachment, anchored, raw_eye[0], raw_eye[1], raw_eye[2],
                    final_eye[0], final_eye[1], final_eye[2],
                    std::hypot(raw_eye[0]-final_eye[0], raw_eye[2]-final_eye[2]));
      }
#endif
    }
  }

#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  const bool relevant = cover == 1 ||
                        (requested_age >= 0 && requested_age <= 2000) ||
                        (composed_age >= 0 && composed_age <= 2000);
  auto& last_log_ms = caller == 0x8220686Cu ? last_log_6c_ms : last_log_b8_ms;
  int64_t previous = last_log_ms.load(std::memory_order_relaxed);
  if (entry <= 8 ||
      (relevant && now - previous >= 100 &&
       last_log_ms.compare_exchange_strong(previous, now,
                                           std::memory_order_relaxed))) {
    REXLOG_INFO(
        "PDZ final cover diagnostic: caller={:08X} entry={} root={:08X} "
        "matrix={:08X} root_player={:08X} cover={} requested_player={:08X} "
        "requested_age={} composed_player={:08X} composed_age={} "
        "yaw_delta={:.4f} pitch_delta={:.4f} result={}",
        caller, entry, camera_root, matrix_address, player, cover,
        requested.player, requested_age, composed.player, composed_age,
        yaw_delta, pitch_delta, reason);
    rex::FlushLogging();
  }
#endif
}

void PDZ_AdjustCoverTargetRay(uint8_t* base,uint32_t player,
                             uint32_t native_origin,uint32_t ray_start,uint32_t ray_end,
                             double& start_x,double& start_y,double& start_z) {
  if(!base||!rex::cvar::Query<bool>("pdz_cover_camera_stabilization")||
     !GuestReadable(base,player,0x16BA)||HudWord(base,player)!=0x820C93A0u||
     !base[player+0x16B9]||base[player+0x16A3]||
     !GuestReadable(base,native_origin,12)||!GuestReadable(base,ray_start,12)||
     !GuestReadable(base,ray_end,12)) return;
  CoverTargetCamera camera;
  bool steady_cover=false;
  {
    std::lock_guard lock(cover_aim_mutex);
    camera=cover_target_camera;
    steady_cover=cover_composed_state.player==player&&cover_composed_state.branch==0;
  }
  const auto now=CrosshairClockMs();
  if(!steady_cover||camera.player!=player||now<camera.time||now-camera.time>100||
     camera.camera!=HudWord(base,0x82D68320u)) return;
  std::array<float,3> origin{},start{},end{};
  for(uint32_t i=0;i<3;++i) {
    origin[i]=rex::memory::load_and_swap<float>(base+native_origin+4*i);
    start[i]=rex::memory::load_and_swap<float>(base+ray_start+4*i);
    end[i]=rex::memory::load_and_swap<float>(base+ray_end+4*i);
  }
  if(!pdz::AlignCoverTargetRay(camera.eye,origin,start,end)) return;
  for(uint32_t i=0;i<3;++i) {
    rex::memory::store_and_swap<float>(base+ray_start+4*i,start[i]);
    rex::memory::store_and_swap<float>(base+ray_end+4*i,end[i]);
  }
  // Native code keeps these near-point components in registers as well.
  start_x=start[0]; start_y=start[1]; start_z=start[2];
  PDZ_TraceCoverAimOrder(base,player,2,ray_start,ray_end,3,native_origin);
  if(cover_acquisition.player==player && cover_acquisition.camera==camera.camera &&
     cover_acquisition.aim_camera==camera.aim_camera &&
     now>=cover_acquisition.time && now-cover_acquisition.time<=100 &&
     GuestReadable(base,camera.aim_camera+112,64)) {
    for(unsigned i=0;i<16;++i)
      cover_acquisition.aim_view[i]=rex::memory::load_and_swap<float>(base+camera.aim_camera+112+4*i);
    // Same orientation as native unprojection; only undo presentation follow.
    pdz::SetCoverViewEye(cover_acquisition.aim_view,camera.eye);
    cover_acquisition_ray=pdz::ValidCoverView(cover_acquisition.aim_view);
  }
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  static thread_local int64_t next_log=0;
  if(now>=next_log) {
    next_log=now+250;
    REXLOG_INFO("PDZ cover target ray aligned: player={:08X} camera_age={} "
                "origin=({:.3f},{:.3f},{:.3f}) camera=({:.3f},{:.3f},{:.3f})",
                player,now-camera.time,origin[0],origin[1],origin[2],
                camera.eye[0],camera.eye[1],camera.eye[2]);
  }
#endif
}

void PDZ_AdjustProjectionFrustum(double& left, double& right,
                                 double& bottom, double& top,
                                 double near_plane, double far_plane,
                                 uint32_t caller) {
  double scale = rex::cvar::Query<double>("pdz_fov_scale");

#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  // Record each call site once. The return address is considerably safer than
  // guessing from near/far-plane values: playtesting showed that HUD,
  // picture-in-picture and weapon passes can all use short frusta.
  static std::mutex projection_trace_mutex;
  static std::unordered_set<uint64_t> projection_callers;
  {
    const bool centered = left < 0.0 && right > 0.0 && bottom < 0.0 && top > 0.0;
    int32_t far_bucket = 0;
    if (std::isfinite(far_plane)) {
      far_bucket = int32_t(std::clamp(std::lround(far_plane * 10.0),
                                      long(INT32_MIN), long(INT32_MAX)));
    }
    const uint64_t trace_key = (uint64_t(caller) << 32) ^
                               (uint32_t(far_bucket) << 1) ^ uint64_t(centered);
    std::lock_guard<std::mutex> lock(projection_trace_mutex);
    if (projection_callers.size() < 256 &&
        projection_callers.insert(trace_key).second) {
      REXLOG_INFO(
          "PDZ projection caller: lr={:08X}, near={:.6f}, far={:.6f}, left={:.6f}, right={:.6f}, bottom={:.6f}, top={:.6f}",
          caller, near_plane, far_plane, left, right, bottom, top);
    }
  }
#endif

  // A short far plane was initially assumed to identify the first-person
  // weapon pass. Playtesting proved that it identifies HUD/crosshair and some
  // picture-in-picture passes instead. Keep the world-scale hook, but never
  // apply viewmodel compensation until the exact weapon pass is identified.
  (void)near_plane;
  (void)far_plane;
  (void)caller;
  left *= scale;
  right *= scale;
  bottom *= scale;
  top *= scale;
}

void PDZ_AdjustGunVisual(double& value, uint32_t axis) {
  const char* setting = axis == 0 ? "pdz_viewmodel_horizontal_shift"
                                  : "pdz_viewmodel_vertical_shift";
  value += rex::cvar::Query<double>(setting);
}

namespace {
struct SwayAimState {
  uint32_t player=0;
  double vertical=0,horizontal=0,x=0,y=0;
  int64_t visual_ms=0,aim_ms=0;
};
std::mutex sway_aim_mutex;
SwayAimState sway_aim_state;
}
bool PDZ_ReadSwayCrosshair(double& x,double& y) {
  if(!rex::cvar::Query<bool>("pdz_auto_aim") && !rex::cvar::Query<bool>("pdz_classic_free_aim") &&
     (!rex::cvar::Query<bool>("pdz_classic_weapon_sway") ||
      !rex::cvar::Query<bool>("pdz_classic_sway_aim"))) return false;
  if(pdz::UiMouseCaptured()) return false;
  std::lock_guard lock(sway_aim_mutex);
  if(!sway_aim_state.aim_ms || HudTime()-sway_aim_state.aim_ms>120) return false;
  x=sway_aim_state.x; y=sway_aim_state.y;
  return true;
}
bool PDZ_ReadVisualCrosshair(double& x,double& y) {
  static std::mutex visual_mutex;
  static pdz::aim::CursorVisual visual;
  static uint32_t previous_player=0;
  static int64_t previous_ms=0;
  double raw_x=0,raw_y=0;
  const bool available=PDZ_ReadSwayCrosshair(raw_x,raw_y);
  uint32_t player=0;
  { std::lock_guard lock(sway_aim_mutex); player=sway_aim_state.player; }
  std::lock_guard lock(visual_mutex);
  if(!available) { visual.Reset(); previous_player=0; previous_ms=0; return false; }
  const auto now=HudTime();
  if(player!=previous_player) { visual.Reset(); previous_ms=0; }
  CoverTargetCamera camera;
  CoverResolvedAim target;
  {
    std::lock_guard lock(cover_aim_mutex);
    camera=cover_target_camera;target=cover_resolved_aim;
  }
  const bool reproject=rex::cvar::Query<bool>("pdz_cover_camera_stabilization")&&
      camera.player==player && target.player==player &&
      camera.camera==target.camera && camera.aim_camera==target.aim_camera &&
      now>=camera.time && now-camera.time<=100 &&
      now>=target.time && now-target.time<=100;
  // Match the complete acquisition rather than combining new input with an old
  // collision depth across the game/render thread boundary.
  if(reproject) { raw_x=target.cursor.x;raw_y=target.cursor.y; }
  auto eased=visual.Update({raw_x,raw_y},previous_ms?(now-previous_ms)/1000.:0.,
                                rex::cvar::Query<double>("pdz_cursor_visual_smoothing"));
  if(reproject) pdz::ReprojectCoverCursor(target.aim_view,target.projection,target.depth,
                                         camera.display_view,camera.projection,eased);
  previous_player=player;previous_ms=now;x=eased.x;y=eased.y;
  return true;
}
void PDZ_AdjustWeaponSpread(uint8_t* base,uint32_t player,uint32_t definition,double& spread) {
  if(!rex::cvar::Query<bool>("pdz_weapon_accuracy")||!base||
     !HudPointer(player,8208)||HudWord(base,player)!=0x820C93A0u||
     !GuestReadable(base,definition,1148)||LocalPlayer(base)!=player) return;
  bool held=false;
  for(uint32_t slot:{4076u,4080u}) {
    const auto item=HudWord(base,player+slot);
    if(GuestReadable(base,item,864)&&HudWord(base,item+92)==player&&
       HudWord(base,item+860)==definition) held=true;
  }
  if(held) spread=pdz::accuracy::Spread(spread,true,HudWord(base,definition+1144));
}
namespace {
thread_local pdz::aim::Retention aim_retention;
thread_local uint32_t retention_player=0,retention_assist=0,retention_weapon=0;
std::mutex assisted_aim_mutex;
struct AssistedAim { uint32_t player=0; bool applied=false; int64_t time=0; } assisted_aim;
bool RetentionContext(uint8_t* base,uint32_t assist) {
  if(!base||!rex::cvar::Query<bool>("pdz_auto_aim")||pdz::UiMouseCaptured()||!HudPointer(assist,120)) {
    aim_retention.Reset();return false;
  }
  const uint32_t player=HudWord(base,assist+12);
  if(!HudPointer(player,8208)||HudWord(base,player)!=0x820C93A0u||
     base[player+0x16A3]||base[player+0x16B9]||ScopeActive(base,player)) {
    aim_retention.Reset();return false;
  }
  const uint32_t root=HudWord(base,0x82E35468u),manager=HudWord(base,0x82D68320u);
  if(!HudPointer(root,0x3BC)||HudWord(base,root+0x3B8)!=player||!HudPointer(manager,0x540)) {
    aim_retention.Reset();return false;
  }
  const uint32_t mode=HudWord(base,manager+0x53C);
  if(!HudPointer(mode,12)||base[mode+11]!=0) { aim_retention.Reset();return false; }
  const uint32_t weapon=HudWord(base,player+4080);
  if(retention_player!=player||retention_assist!=assist||retention_weapon!=weapon)aim_retention.Reset();
  retention_player=player;retention_assist=assist;retention_weapon=weapon;
  return true;
}
}
void PDZ_AdjustAimRetentionBounds(uint8_t* base,uint32_t assist,uint32_t candidate,
                                 uint32_t rectangle,double x,double y,double scale_x,double scale_y) {
  if(!RetentionContext(base,assist)||!aim_retention.Active(candidate,HudTime())||
     !HudPointer(rectangle,16)||!std::isfinite(scale_x)||!std::isfinite(scale_y))return;
  const uint32_t player=retention_player;
  pdz::aim::Point offset;
  if(!ReadFreeAim(player,offset)&&rex::cvar::Query<bool>("pdz_classic_weapon_sway")&&
     rex::cvar::Query<bool>("pdz_classic_sway_aim")&&
     pdz::aim::ManualSwayActive(rex::cvar::Query<bool>("pdz_sway_during_ads"),AimHeld(base,player))) {
    const uint32_t controller=HudWord(base,player+6560);
    const uint32_t camera=HudPointer(controller,1152)?HudWord(base,controller+1148):0;
    pdz::aim::Projection projection;
    if(HudPointer(camera,104)&&ReadAimProjection(base,camera,projection)) {
      std::lock_guard lock(sway_aim_mutex);
      if(sway_aim_state.player==player&&sway_aim_state.visual_ms&&HudTime()-sway_aim_state.visual_ms<=120)
        pdz::aim::WeaponEulerOffset({sway_aim_state.horizontal,sway_aim_state.vertical},projection,offset);
    }
  }
  const auto f=[&](uint32_t at){return double(rex::memory::load_and_swap<float>(base+at));};
  pdz::aim::RetentionRect rect{f(rectangle),f(rectangle+4),f(rectangle+8),f(rectangle+12)};
  if(!aim_retention.Expand(candidate,HudTime(),x,y,offset.x*scale_x,-offset.y*scale_y,rect))return;
  rex::memory::store_and_swap<float>(base+rectangle,float(rect.x));
  rex::memory::store_and_swap<float>(base+rectangle+4,float(rect.y));
  rex::memory::store_and_swap<float>(base+rectangle+8,float(rect.width));
  rex::memory::store_and_swap<float>(base+rectangle+12,float(rect.height));
}
void PDZ_ApplyAutoAimSetting(uint8_t* base,uint32_t player) {
  if(!base||!HudPointer(player,8208)||
     HudWord(base,player)!=0x820C93A0u||base[player+0x16A3]) return;
  const bool scoped=ScopeActive(base,player);
  if(pdz::aim::UseNativePolicy(rex::cvar::Query<bool>("pdz_auto_aim"),
     rex::cvar::Query<bool>("pdz_native_aim_assist"),scoped)) return;
  if(pdz::settings::AllowAimAssist(rex::cvar::Query<bool>("pdz_auto_aim"),scoped)) return;
  const uint32_t root=HudWord(base,0x82E35468u);
  if(!HudPointer(root,0x3BC)||HudWord(base,root+0x3B8)!=player) return;
  // Exact producer already initialized these to zero, then copied selected
  // assist coordinates into them. Remove only that contribution, before
  // per-weapon spread/unprojection. Free aim is added later independently.
  rex::memory::store_and_swap<float>(base+player+8024,0.f);
  rex::memory::store_and_swap<float>(base+player+8028,0.f);
}
uint32_t PDZ_AutoAimApplicationMode(uint8_t* base,uint32_t player,uint32_t original,
                                   double& attraction_strength,uint32_t assist) {
  if(!base||
     !HudPointer(player,8208)||HudWord(base,player)!=0x820C93A0u||
     base[player+0x16A3]||pdz::UiMouseCaptured()) return original;
  const uint32_t root=HudWord(base,0x82E35468u),manager=HudWord(base,0x82D68320u);
  if(!HudPointer(root,0x3BC)||HudWord(base,root+0x3B8)!=player||!HudPointer(manager,0x540)) return original;
  const uint32_t mode=HudWord(base,manager+0x53C);
  if(!HudPointer(mode,12)||(base[mode+11]!=0&&base[mode+11]!=3)) return original;
  const bool enabled=rex::cvar::Query<bool>("pdz_auto_aim");
  const bool scoped=ScopeActive(base,player);
  if(pdz::aim::UseNativePolicy(enabled,rex::cvar::Query<bool>("pdz_native_aim_assist"),scoped))
    return original;
  const uint32_t application=pdz::aim::OverlayAimMode(original,true,enabled,scoped);
  if(application!=1) return application;
  const bool retaining=RetentionContext(base,assist)&&
      aim_retention.Active(HudWord(base,assist+20),HudTime());
  // Native selector has already checked candidate eligibility and LOS. Its
  // attraction stage requires controller+1172==1; live mode was0.
  // Overlay policy owns this comparison for either native setting, never the
  // stored mode, profile setting or shared getter. Input source is irrelevant.
  const double before=attraction_strength;
  attraction_strength=pdz::aim::OverlayAimStrength(before,true,enabled,scoped);
  if(rex::cvar::Query<bool>("pdz_auto_aim_range_boost")&&HudPointer(assist,120)&&
     HudWord(base,assist+12)==player&&HudWord(base,assist+20)) {
    // Native 824E2028 copies the selected world point to +36 and input ray
    // origin to +64 before this gate. No actor position/height is guessed.
    const auto f=[base](uint32_t p){return double(rex::memory::load_and_swap<float>(base+p));};
    const double distance=std::hypot(f(assist+36)-f(assist+64),
        f(assist+40)-f(assist+68),f(assist+44)-f(assist+72));
    attraction_strength*=pdz::aim::CloseRangeAttraction(distance,
        rex::cvar::Query<double>("pdz_auto_aim_close_boost"));
  }
  if(retaining)attraction_strength=std::max(attraction_strength,1.5);
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  static std::atomic<bool> logged{false};
  if(!logged.exchange(true))
    REXLOG_INFO("PDZ mouse auto-aim adapter active: player={:08X} mode={}->{} attraction={:.6f}->{:.6f}",
                player,original,application,before,attraction_strength);
#endif
  return application;
}
void PDZ_ObserveAutoAim(uint8_t* base,uint32_t assist,bool applied) {
  {
    std::lock_guard lock(assisted_aim_mutex);
    assisted_aim={base&&HudPointer(assist,120)?HudWord(base,assist+12):0,applied,HudTime()};
  }
  if(RetentionContext(base,assist))
    aim_retention.Observe(HudWord(base,assist+20),applied,HudTime());
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
  // Read at the native function exit, not from an asynchronous half-written
  // snapshot. r22 records actual native acceptance/projection, including LOS.
  if(!base||!HudPointer(assist,120)) return;
  const uint32_t player=HudWord(base,assist+12);
  if(!HudPointer(player,8208)||HudWord(base,player)!=0x820C93A0u||
     base[player+0x16A3]) return;
  const uint32_t root=HudWord(base,0x82E35468u);
  if(!HudPointer(root,0x3BC)||HudWord(base,root+0x3B8)!=player) return;
  static thread_local int64_t next_log=0;
  static thread_local unsigned baseline_logs=0,accept_logs=0;
  static thread_local bool previous_applied=false;
  static thread_local bool previous_firing=false;
  static thread_local uint32_t previous_selected=0;
  static thread_local unsigned firing_logs=0;
  const int64_t now=HudTime();
  const bool accepted_edge=applied&&!previous_applied&&accept_logs<8;
  previous_applied=applied;
  const bool firing=(weapon_input.load()>>16)>=30||weapon_right_trigger.load()>=30;
  const uint32_t selected=HudWord(base,assist+20);
  const bool firing_sample=firing_logs<120 &&
      (firing!=previous_firing || (firing&&(selected!=previous_selected||now>=next_log)));
  previous_firing=firing; previous_selected=selected;
  if(!accepted_edge&&!firing_sample&&(now<next_log||baseline_logs>=12)) return;
  if(firing_sample)++firing_logs;
  if(accepted_edge) ++accept_logs; else ++baseline_logs;
  next_log=now+(firing?100:2000);
  const auto f=[base,assist](uint32_t offset){return rex::memory::load_and_swap<float>(base+assist+offset);};
  REXLOG_INFO("PDZ auto-aim completion: player={:08X} enabled={} selected={:08X} applied={} motion=({:.6f},{:.6f}) weapon={:.6f} point=({:.6f},{:.6f})",
              player,rex::cvar::Query<bool>("pdz_auto_aim"),HudWord(base,assist+20),
              applied,f(92),f(96),f(100),f(56),f(60));
  AimInputTrace input;
  {
    std::lock_guard lock(aim_input_trace_mutex);
    input=aim_input_trace;
  }
  const bool fresh=input.player==player &&
      std::chrono::steady_clock::now()-input.time<=std::chrono::milliseconds(100);
  pdz::aim::Point free_point;
  const bool free=ReadFreeAim(player,free_point);
  double sway_x=0,sway_y=0;
  {
    std::lock_guard lock(sway_aim_mutex);
    if(sway_aim_state.player==player && now-sway_aim_state.visual_ms<=120) {
      sway_x=sway_aim_state.horizontal;sway_y=sway_aim_state.vertical;
    }
  }
  REXLOG_INFO("PDZ aim diagnostic: firing={} scoped={} input_fresh={} gyro_deg=({:.6f},{:.6f}) stick_deg=({:.6f},{:.6f}) free={} offset=({:.6f},{:.6f}) sway_rad=({:.6f},{:.6f})",
      firing,ScopeActive(base,player),fresh,input.gyro_x,input.gyro_y,
      input.stick_x,input.stick_y,free,free_point.x,free_point.y,sway_x,sway_y);
#endif
}
void PDZ_ApplySwayAim(uint8_t* base,uint32_t player,uint32_t camera,
                      uint32_t screen_point,double native_x,double native_y,double zoom) {
  cover_acquisition={};cover_acquisition_ray=false;
  if(!base || !HudPointer(player,8208)||HudWord(base,player)!=0x820C93A0u||
     !HudPointer(camera,104) || !HudPointer(screen_point,8) ||
     !std::isfinite(zoom) || zoom<=0) return;
  pdz::aim::Point offset;
  const bool free=ReadFreeAim(player,offset);
  std::lock_guard lock(sway_aim_mutex);
  const uint32_t root=HudWord(base,0x82E35468u);
  if(!HudPointer(root,0x3BC)||HudWord(base,root+0x3B8)!=player) return;
  const auto value=[base](uint32_t p){return double(rex::memory::load_and_swap<float>(base+p));};
  const uint32_t manager=HudWord(base,0x82D68320u);
  if(!HudPointer(manager,0x540)) return;
  const uint32_t mode=HudWord(base,manager+0x53C);
  if(!HudPointer(mode,12)||(base[mode+11]!=0&&base[mode+11]!=3)||base[player+0x16A3]) return;
  pdz::aim::Projection projection;
  if(!ReadAimProjection(base,camera,projection)) return;
  PDZ_TraceCoverAimOrder(base,player,7,camera+68,0,2,camera);
  const uint32_t trace_display=HudWord(base,0x82D6B2BCu);
  PDZ_TraceCoverAimOrder(base,player,8,camera+96,trace_display+104,2,trace_display);
  if(!free) {
    offset={};
    const bool turn_sway=base[mode+11]==0 && value(player+0x1910)<=1.001 &&
        pdz::aim::ManualSwayActive(rex::cvar::Query<bool>("pdz_sway_during_ads"),AimHeld(base,player)) &&
        rex::cvar::Query<bool>("pdz_classic_weapon_sway")&&rex::cvar::Query<bool>("pdz_classic_sway_aim")&&
        sway_aim_state.player==player&&sway_aim_state.visual_ms&&HudTime()-sway_aim_state.visual_ms<=120;
    if(turn_sway&&!pdz::aim::WeaponEulerOffset({sway_aim_state.horizontal,sway_aim_state.vertical},projection,offset)) return;
  }
  bool assisted=false;
  {
    std::lock_guard lock(assisted_aim_mutex);
    assisted=pdz::aim::AssistanceOwnsAim(
        rex::cvar::Query<bool>("pdz_auto_aim")&&
        !ScopeActive(base,player)&&!pdz::UiMouseCaptured(),player,assisted_aim.player,
        assisted_aim.applied,assisted_aim.time,HudTime());
  }
  const double dx=assisted?0:offset.x,dy=assisted?0:offset.y;
  const double original_x=value(screen_point),original_y=value(screen_point+4);
  if(!std::isfinite(original_x)||!std::isfinite(original_y)||!std::isfinite(dx)||!std::isfinite(dy)||
     !std::isfinite(native_x)||!std::isfinite(native_y)) return;
  if(dx!=0||dy!=0) {
    rex::memory::store_and_swap<float>(base+screen_point,float(std::clamp(original_x+dx,-1.,1.)));
    rex::memory::store_and_swap<float>(base+screen_point+4,float(std::clamp(original_y+dy,-1.,1.)));
  }
  sway_aim_state.player=player;
  sway_aim_state.x=std::clamp(native_x/zoom+dx,-1.,1.);
  sway_aim_state.y=std::clamp(native_y/zoom+dy,-1.,1.);
  sway_aim_state.aim_ms=HudTime();
  if(rex::cvar::Query<bool>("pdz_cover_camera_stabilization")&&
     base[mode+11]==3 && base[player+0x16B9]==1 && AimHeld(base,player)) {
    cover_acquisition.player=player;cover_acquisition.camera=manager;
    cover_acquisition.aim_camera=camera;cover_acquisition.projection=projection;
    cover_acquisition.cursor={sway_aim_state.x,sway_aim_state.y};
    cover_acquisition.time=sway_aim_state.aim_ms;
  }
  PDZ_TraceCoverAimOrder(base,player,1,screen_point,0,2,camera);
}
void PDZ_ApplyClassicWeaponSway(uint8_t* base, uint32_t player,
                              double& vertical, double& horizontal) {
  // Only the proven viewmodel rotation assembly calls this. Apply gun Euler
  // offsets here; the separate native aim hook owns the reticle/shot point.
  static thread_local pdz::WeaponSway sway;
  static thread_local pdz::aim::CursorVisual weapon_visual;
  static thread_local uint32_t visual_player=0,visual_camera=0,visual_weapon=0;
  static thread_local std::chrono::steady_clock::time_point visual_time{};
  static thread_local uint32_t previous_player=0, previous_camera=0;
  static thread_local std::chrono::steady_clock::time_point previous_time{};
  const auto reset=[&] { sway.Reset(); previous_player=previous_camera=0; previous_time={};
    weapon_visual.Reset(); visual_player=visual_camera=visual_weapon=0; visual_time={};
    std::lock_guard lock(sway_aim_mutex);
    sway_aim_state.visual_ms=0;sway_aim_state.vertical=sway_aim_state.horizontal=0; };
  if (!base) { reset(); return; }
  const auto readable=[base](uint32_t p,size_t n) {
    if(p<0x10000u || n==0 || uint64_t(p)+n>UINT32_MAX) return false;
    size_t length=n; rex::memory::PageAccess access=rex::memory::PageAccess::kNoAccess;
    return rex::memory::QueryProtect(base+p,length,access) && length>=n &&
           access!=rex::memory::PageAccess::kNoAccess;
  };
  const auto word=[base](uint32_t p) { return rex::memory::load_and_swap<uint32_t>(base+p); };
  const auto number=[base](uint32_t p) { return double(rex::memory::load_and_swap<float>(base+p)); };
  if(!readable(player,0x2000) || word(player)!=0x820C93A0u || base[player+0x16A3]) { reset(); return; }
  if(!pdz::aim::ManualSwayActive(rex::cvar::Query<bool>("pdz_sway_during_ads"),AimHeld(base,player))) { reset(); return; }
  if(!std::isfinite(vertical)||!std::isfinite(horizontal)) { reset(); return; }
  const uint32_t root=word(0x82E35468u), camera=word(0x82D68320u);
  if(!readable(root,0x3BC) || word(root+0x3B8)!=player || !readable(camera,0x540)) { reset(); return; }
  const uint32_t mode=word(camera+0x53C);
  const double zoom=number(player+0x1910);
  const auto ease_weapon=[&](pdz::aim::Point target) {
    const auto now=std::chrono::steady_clock::now();
    const uint32_t weapon=word(player+4080u);
    double seconds=visual_time.time_since_epoch().count()?
        std::chrono::duration<double>(now-visual_time).count():0.;
    if(visual_player!=player||visual_camera!=camera||visual_weapon!=weapon||seconds>.25) {
      weapon_visual.Reset(); seconds=0.;
    }
    // Start from neutral; smooth only render Euler offsets, never shot coordinates.
    if(!weapon_visual.initialized) weapon_visual.Update({},0.,.05);
    const auto eased=weapon_visual.Update(target,seconds,.05);
    visual_player=player; visual_camera=camera; visual_weapon=weapon; visual_time=now;
    return eased;
  };
  pdz::aim::Point free_point;
  bool assisted=false;
  {
    std::lock_guard lock(assisted_aim_mutex);
    assisted=pdz::aim::AssistanceOwnsAim(
        rex::cvar::Query<bool>("pdz_auto_aim")&&!ScopeActive(base,player)&&!pdz::UiMouseCaptured(),
        player,assisted_aim.player,assisted_aim.applied,assisted_aim.time,HudTime());
  }
  if(assisted) {
    // Use the accepted final aim point, not manual input or camera recoil.
    std::lock_guard lock(sway_aim_mutex);
    assisted=sway_aim_state.player==player&&sway_aim_state.aim_ms&&
        HudTime()-sway_aim_state.aim_ms<=100;
    if(assisted) free_point={sway_aim_state.x,sway_aim_state.y};
  }
  if((assisted||ReadFreeAim(player,free_point)) && readable(mode,12) && base[mode+11]==0) {
    const uint32_t controller=word(player+6560u);
    const uint32_t aim_camera=readable(controller+1148u,4)?word(controller+1148u):0;
    pdz::aim::Projection projection;
    if(readable(aim_camera,104)&&ReadAimProjection(base,aim_camera,projection)) {
      // Same validated conversion, with render-only 50ms exponential easing.
      const auto angles=ease_weapon({
          -std::atan(free_point.x*projection.tan_half_vertical*projection.aspect),
          -std::atan(free_point.y*projection.tan_half_vertical)});
      horizontal+=angles.x;
      vertical+=angles.y;
      sway.Reset(); previous_time={}; previous_player=previous_camera=0;
      return;
    }
  }
  // First-person on foot only; keep cover, gadgets, vehicle cameras and scope
  // magnification original. No HUD visibility heuristic or projection hook.
  if(!readable(mode,12) || base[mode+11]!=0 || !std::isfinite(zoom) || zoom>1.001 ||
     !std::isfinite(vertical) || !std::isfinite(horizontal)) { reset(); return; }
  // Ease the assisted/free-aim offset back to neutral on release as well.
  const auto released=ease_weapon({});
  horizontal+=released.x; vertical+=released.y;
  if(!rex::cvar::Query<bool>("pdz_classic_weapon_sway")) {
    sway.Reset(); previous_time={}; previous_player=previous_camera=0;
    return;
  }
  const double x=-number(camera+1024+8), y=-number(camera+1024+24), z=-number(camera+1024+40);
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||std::abs(std::hypot(x,y,z)-1)>.05) { reset(); return; }
  double yaw=std::atan2(x,-z), pitch=std::asin(std::clamp(y,-1.,1.));
  // Direct input has an authoritative pre-recoil turn source. Camera recoil,
  // damage shake and auto-aim corrections must not kick the custom cursor.
  if(rex::cvar::Query<bool>("pdz_direct_mouse")||
     rex::cvar::Query<std::string>("pdz_stick_curve")!="original"||
     rex::cvar::Query<std::string>("pdz_gyro_mode")!="off") {
    std::lock_guard lock(aim_input_trace_mutex);
    if(sway_input.player!=player) { reset();return; }
    yaw=sway_input.yaw;pitch=sway_input.pitch;
  }
  const auto now=std::chrono::steady_clock::now();
  double seconds=previous_time.time_since_epoch().count()?std::chrono::duration<double>(now-previous_time).count():.016;
  if(previous_player!=player || previous_camera!=camera) sway.Reset();
  if(seconds>=.001 || !sway.initialized) {
    sway.Update(yaw,pitch,seconds,rex::cvar::Query<double>("pdz_classic_weapon_sway_amount"));
    previous_time=now; previous_player=player; previous_camera=camera;
  }
  const auto filtered=pdz::aim::SwayDeadzone({sway.horizontal,sway.vertical},
      rex::cvar::Query<double>("pdz_cursor_sway_deadzone"));
  vertical+=filtered.y;
  horizontal+=filtered.x;
  std::lock_guard lock(sway_aim_mutex);
  sway_aim_state.player=player;
  sway_aim_state.vertical=filtered.y; sway_aim_state.horizontal=filtered.x;
  sway_aim_state.visual_ms=HudTime();
}

void PDZ_UpdateLiveFov() {
  auto* runtime = rex::Runtime::instance();
  uint8_t* base = runtime ? runtime->virtual_membase() : nullptr;
  if (!base) return;

  constexpr double kPi = 3.14159265358979323846;
  constexpr double kAspect16By9 = 16.0 / 9.0;
  const double horizontal =
      rex::cvar::Query<double>("pdz_horizontal_fov") * kPi / 180.0;
  const float requested_vertical = static_cast<float>(
      2.0 * std::atan(std::tan(horizontal * 0.5) / kAspect16By9) *
      180.0 / kPi);
  if (!std::isfinite(requested_vertical)) return;

  auto load_u32 = [base](uint32_t address) {
    return uint32_t(*reinterpret_cast<rex::be<uint32_t>*>(base + address));
  };
  auto load_f32 = [&load_u32](uint32_t address) {
    PPCRegister value{};
    value.u32 = load_u32(address);
    return value.f32;
  };
  auto store_f32 = [base](uint32_t address, float value) {
    PPCRegister bits{};
    bits.f32 = value;
    *reinterpret_cast<rex::be<uint32_t>*>(base + address) = bits.u32;
  };

  static float applied_vertical = 0.0f;
  if (std::abs(applied_vertical - requested_vertical) < 0.001f) return;

  constexpr uint32_t kDefaultFovAddress = 0x820EC228u;
  const float previous_default = load_f32(kDefaultFovAddress);
  const uint32_t fov_root = load_u32(0x82D68320u);
  size_t queried_length = 4;
  rex::memory::PageAccess queried_access = rex::memory::PageAccess::kNoAccess;
  const bool valid_root = fov_root <= UINT32_MAX - 0x444u &&
      rex::memory::QueryProtect(base + fov_root + 0x440u, queried_length,
                                queried_access) &&
      queried_access != rex::memory::PageAccess::kNoAccess;

  // Preserve scope magnification if the active FOV is valid.
  if (valid_root && previous_default > 1.0f) {
    const uint32_t active_address = fov_root + 0x440u;
    const float active = load_f32(active_address);
    if (std::isfinite(active) && active > 0.01f && active < 180.0f) {
      store_f32(active_address,
                requested_vertical * (active / previous_default));
    }
  }

  rex::memory::PageAccess old_access{};
  rex::memory::Protect(base + kDefaultFovAddress, 4,
                       rex::memory::PageAccess::kReadWrite, &old_access);
  store_f32(kDefaultFovAddress, requested_vertical);
  rex::memory::Protect(base + kDefaultFovAddress, 4, old_access, nullptr);
  applied_vertical = requested_vertical;
}


#if defined(PDZ_BUILD_COVER_RENDER_DIAGNOSTIC)
// Separate diagnostic candidate only, excluded from ordinary/audio-fix builds.
// Opt-in diagnostic only: no guest register, memory or GPU state modification.
REXCVAR_DEFINE_BOOL(pdz_cover_render_trace, false, "Diagnostics",
                   "Bounded Joanna cover submission/draw trace; requires restart to rearm");
namespace {
struct CoverDrawDiagnostic {
  uint32_t instance=0, geometry=0, caller=0, pass=0;
  bool joanna=false;
};
thread_local CoverDrawDiagnostic cover_draw_diagnostic;
thread_local bool cover_trace_neighbor=false;
std::atomic<unsigned> cover_trace_submissions{0}, cover_trace_draws{0};
}
REX_EXTERN(__imp__sub_827017F8);
REX_HOOK_RAW(sub_827017F8) {
  const auto saved=cover_draw_diagnostic;
  cover_draw_diagnostic={};
  const uint32_t caller=static_cast<uint32_t>(ctx.lr);
  if(base&&rex::cvar::Query<bool>("pdz_cover_render_trace")&&
     rex::cvar::Query<bool>("pdz_cover_camera_stabilization")&&
     (caller==0x82407058u||caller==0x824070A0u||caller==0x824072E8u)) {
    const uint32_t root=GuestReadable(base,0x82E35468u,4)?HudWord(base,0x82E35468u):0;
    const uint32_t player=GuestReadable(base,root,0x3BC)?HudWord(base,root+0x3B8):0;
    if(GuestReadable(base,player,0x16BA)&&HudWord(base,player)==0x820C93A0u&&base[player+0x16B9]) {
      const uint32_t instance=caller==0x824072E8u?ctx.r29.u32:ctx.r30.u32;
      const bool joanna=instance==HudWord(base,player+72)||
                        (instance&&instance==HudWord(base,player+4104));
      const bool neighbor=!joanna&&cover_trace_neighbor;
      if(joanna||neighbor) {
        const unsigned sequence=cover_trace_submissions.fetch_add(1);
        if(sequence<64) {
          cover_draw_diagnostic={instance,ctx.r3.u32,caller,ctx.r5.u32,joanna};
          REXLOG_INFO("PDZ cover submission seq={} joanna={} instance={:08X} geometry={:08X} caller={:08X} mask={:08X}",
                      sequence,joanna,instance,ctx.r3.u32,caller,ctx.r5.u32);
        }
      }
      cover_trace_neighbor=joanna;
    } else cover_trace_neighbor=false;
  }
  __imp__sub_827017F8(ctx,base);
  cover_draw_diagnostic=saved;
}
REX_EXTERN(__imp__sub_826DA3D8);
REX_HOOK_RAW(sub_826DA3D8) {
  // Snapshot pending native state before the draw helper flushes it. Raw words
  // are not decoded blend/depth claims; capture replay supplies final GPU state.
  const auto attribution=cover_draw_diagnostic;
  const uint32_t render=ctx.r3.u32;
  const bool record=attribution.instance&&GuestReadable(base,render,20460)&&
                    cover_trace_draws.fetch_add(1)<256;
  const uint32_t begin=record?HudWord(base,render):0;
  if(record) REXLOG_INFO("PDZ cover draw joanna={} instance={:08X} geometry={:08X} caller={:08X} command={:08X} packet_begin={:08X} shader_ref11408={:08X} shader_ref20456={:08X} state11548={:08X} state11636={:08X} state11652={:08X}",
    attribution.joanna,attribution.instance,attribution.geometry,attribution.caller,
    ctx.r31.u32,begin,HudWord(base,render+11408),HudWord(base,render+20456),
    HudWord(base,render+11548),HudWord(base,render+11636),HudWord(base,render+11652));
  __imp__sub_826DA3D8(ctx,base);
  if(record) REXLOG_INFO("PDZ cover draw packets instance={:08X} begin={:08X} end={:08X}",
                        attribution.instance,begin,HudWord(base,render));
}
#endif
