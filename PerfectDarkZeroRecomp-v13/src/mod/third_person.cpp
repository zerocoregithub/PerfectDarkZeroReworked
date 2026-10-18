// Third-person camera mod. See README.md next to this file.
//
// Mode id 0 (vtable 82097CF8, update sub_8220D9C8) is the gameplay camera. It
// carries a camera-local offset vector at +160/+164/+168: when it is nonzero
// the camera is moved by it (with a collision ray pulling it back in). Rolls
// animate this offset (~cm, z < 0 = behind) and reset it to zero after.
// Upstream supplies the third-person foundation; Reworked adds shoulder easing,
// input combinations and scoped native-eye transitions. Offsets are refreshed
// during gameplay, but weapon-scoped ADS eases them back to the native eye and
// restores the chosen shoulder on release. Clearance publishes the native eye
// for pdz_qol.cpp's target check; it does not relocate the native muzzle.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>

#include <rex/hook.h>
#include <rex/ui/keybinds.h>

#include "../game_constants.h"
#include "../game_cvars.h"
#include "third_person.h"
#include "../pdz_shoulder_swap.h"
#include "../pdz_qol.h"

REX_EXTERN(__imp__sub_8220D9C8);
REX_EXTERN(__imp__sub_82637BA0);
REX_EXTERN(__imp__sub_825B2190);
REX_EXTERN(__imp__sub_826285D0);
REX_EXTERN(sub_82252798);

namespace
{
  uint32_t LoadBe32(const uint8_t *base, uint32_t address)
  {
    const uint8_t *p = base + address;
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
  }

  float LoadBeFloat(const uint8_t *base, uint32_t address)
  {
    const uint32_t bits = LoadBe32(base, address);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }

  void StoreBeFloat(uint8_t *base, uint32_t address, float value)
  {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    uint8_t *p = base + address;
    p[0] = uint8_t(bits >> 24);
    p[1] = uint8_t(bits >> 16);
    p[2] = uint8_t(bits >> 8);
    p[3] = uint8_t(bits);
  }

  // Gameplay camera mode fields. Before its update: the offset vector and the
  // vertical FOV (58 degrees * player zoom factor, from the previous frame).
  // After its update: final camera position, look point (eye + view rotation *
  // (0, 0, kLookDistance)) and this frame's eye position.
  constexpr uint32_t kCameraPosition = 4;
  constexpr uint32_t kLookPoint = 16;
  constexpr uint32_t kFov = 112;
  constexpr uint32_t kOffset = 160;
  constexpr uint32_t kEyePosition = 188;
  constexpr double kLookDistance = 1000.0;

  // Zoom is detected from the FOV: enter as soon as it drops, leave only once
  // it is fully back; the gap keeps the camera from flapping at a threshold.
  constexpr float kBaseFov = 58.0f;
  constexpr float kZoomEnterFov = kBaseFov - 1.0f;
  constexpr float kZoomExitFov = kBaseFov - 0.3f;
  bool g_zoomed = false;

  // pdz_tp_zoom_mode values.
  constexpr int32_t kZoomKeep = 0;
  constexpr int32_t kZoomFirstPerson = 1;
  constexpr int32_t kZoomForward = 2;

  // Gameplay camera mode we last wrote our offset into, so that turning the
  // mod off can put that camera back in the head once.
  uint32_t g_offset_mode = 0;
  std::mutex clearance_mutex;
  struct ClearanceEye {uint32_t player=0;float eye[3]{};int64_t time=0;bool active=false;};
  ClearanceEye clearance_eye;

  // Mode id 0 is also used by other camera controllers (the CamSpy has its own).
  // Only the player's camera gets the offset: the player class has
  // sub_8259B6D8 ("camera near the eyes", used by held items) at vtable +744.
  constexpr uint32_t kPlayerVtblCameraNear = 744;
  constexpr uint32_t kPlayerCameraNearFunc = 0x8259B6D8;

  bool IsPlayer(const uint8_t *base, uint32_t object)
  {
    if (!object)
    {
      return false;
    }
    const uint32_t vtbl = LoadBe32(base, object);
    if (vtbl < GameConstants::kImageBase || vtbl >= GameConstants::kImageBase + GameConstants::kImageSize)
    {
      return false;
    }
    return LoadBe32(base, vtbl + kPlayerVtblCameraNear) == kPlayerCameraNearFunc;
  }

  int64_t SteadyMs()
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  // Gadget updates are not called every frame; treat a gadget as in use for a
  // short while after its last update.
  constexpr std::chrono::milliseconds kGadgetHold{150};

  // The Data Thief's hacking screen lives on its first-person model, which is
  // only used while the camera is in the head. Stamped by its update.
  std::atomic<int64_t> g_data_thief_active_ms{INT64_MIN / 2};

  bool DataThiefNeedsFirstPerson()
  {
    return REXCVAR_GET(pdz_tp_auto_fp_gadgets) &&
           SteadyMs() - g_data_thief_active_ms.load(std::memory_order_relaxed) < kGadgetHold.count();
  }

  // Binoculars (AudioScope): object and time of their last update.
  std::atomic<int64_t> g_binoculars_active_ms{INT64_MIN / 2};
  std::atomic<uint32_t> g_binoculars_object{0};

  bool BinocularsActive()
  {
    return SteadyMs() - g_binoculars_active_ms.load(std::memory_order_relaxed) < kGadgetHold.count();
  }

  // The AudioScope keeps two {ref, target} pairs: +1380/+1384 is the lock,
  // +1388/+1392 the target under the reticle. +1384 is set for exactly as long
  // as a lock holds. The target's position is at +48.
  constexpr uint32_t kBinocularsLockTarget = 1384;
  constexpr uint32_t kTargetPosition = 48;

  uint32_t BinocularsLockTarget(const uint8_t *base)
  {
    const uint32_t binoculars = g_binoculars_object.load(std::memory_order_relaxed);
    return binoculars ? LoadBe32(base, binoculars + kBinocularsLockTarget) : 0;
  }

  // Wall collision. The game sweeps a sphere from the eye to the offset camera
  // position and moves a separate camera vector (+172) towards the result at a
  // limited speed (sub_826285D0); the camera is eye + basis * that vector. It
  // could stay caught on walls and ceilings. Instead the mod casts its own ray
  // each frame and writes the camera vector itself.
  constexpr uint32_t kCameraVector = 172;
  // In sub_8220D9C8's frame: the camera-local x, y, z axes in world space.
  constexpr uint32_t kCallerBasis = 240;
  // Ray hit collector, built like sub_8220D9C8 does: vtable, +4 and +108
  // cleared, then vtbl+12 resets it. vtbl+8 = has hit, +80 = hit fraction.
  constexpr uint32_t kRayCollectorVtbl = 0x820925C0;
  constexpr uint32_t kRayLayer = 16; // the game's camera collision layer
  constexpr float kWallMargin = 20.0f; // keep the near plane out of the wall
  constexpr double kEaseOutPerSecond = 8.0;

  // Gameplay camera mode inside its update while it carries our offset.
  uint32_t g_collision_mode = 0;
  // Fraction of the offset in use: snaps in when a wall gets close, eases back
  // out when it is gone.
  float g_fraction = 1.0f;
  bool g_fraction_valid = false;
  std::chrono::steady_clock::time_point g_fraction_time;

  // Returns r3; 0 if the method can't be resolved.
  uint32_t CallVirtual(PPCContext &ctx, uint8_t *base, uint32_t object, uint32_t slot)
  {
    PPCFunc *fn = rex::runtime::ResolveIndirectFunction(LoadBe32(base, LoadBe32(base, object) + slot));
    if (!fn)
    {
      return 0;
    }
    ctx.r3.u64 = object;
    fn(ctx, base);
    return ctx.r3.u32;
  }

  // How far along eye -> end the camera can go before it hits something (0..1).
  float FreeFraction(PPCContext &ctx, uint8_t *base, const float eye[3], const float end[3])
  {
    float length2 = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
      length2 += (end[i] - eye[i]) * (end[i] - eye[i]);
    }
    const float length = std::sqrt(length2);
    if (length < 1.0f)
    {
      return 1.0f;
    }

    // Scratch below the caller's frame; the callees build their frames under sp.
    rex::CallFrame frame(ctx);
    const uint32_t sp = (ctx.r1.u32 - 0x400) & ~0xFu;
    const uint32_t from = sp + 0x200;
    const uint32_t to = sp + 0x210;
    const uint32_t collector = sp + 0x280;
    frame.ctx.r1.u64 = sp;
    for (uint32_t i = 0; i < 3; ++i)
    {
      StoreBeFloat(base, from + i * 4, eye[i]);
      StoreBeFloat(base, to + i * 4, end[i]);
    }
    std::memset(base + collector, 0, 0x100);
    const uint32_t vtbl = kRayCollectorVtbl;
    for (int i = 0; i < 4; ++i)
    {
      base[collector + i] = uint8_t(vtbl >> (24 - 8 * i));
    }
    CallVirtual(frame.ctx, base, collector, 12);

    frame.ctx.r3.u64 = kRayLayer;
    frame.ctx.r4.u64 = from;
    frame.ctx.r5.u64 = to;
    frame.ctx.r6.u64 = collector;
    frame.ctx.r7.u64 = 1;
    sub_82252798(frame.ctx, base);

    if ((CallVirtual(frame.ctx, base, collector, 8) & 0xFF) == 0)
    {
      return 1.0f;
    }
    const float hit = LoadBeFloat(base, collector + 80);
    if (!(hit >= 0.0f && hit <= 1.0f))
    {
      return 1.0f;
    }
    return std::max(0.0f, hit - kWallMargin / length);
  }
} // namespace

bool PDZ_CopyThirdPersonClearanceEye(uint32_t player,float eye[3]) {
  std::lock_guard lock(clearance_mutex);
  const auto age=SteadyMs()-clearance_eye.time;
  if(!clearance_eye.active||clearance_eye.player!=player||age<0||age>100)return false;
  for(unsigned i=0;i<3;++i) {
    if(!std::isfinite(clearance_eye.eye[i]))return false;
    eye[i]=clearance_eye.eye[i];
  }
  return true;
}

REX_HOOK_RAW(sub_8220D9C8)
{
  const uint32_t mode = ctx.r3.u32;
  const uint32_t target = ctx.r4.u32;

  const bool is_player = IsPlayer(base, target);
  const float fov = LoadBeFloat(base, mode + kFov);
  if (is_player && fov > 0.0f)
  {
    g_zoomed = g_zoomed ? fov < kZoomExitFov : fov < kZoomEnterFov;
  }
  // A zoom that starts with the binoculars counts as a binocular zoom until it
  // ends.
  static bool binoculars_zoom = false;
  binoculars_zoom = g_zoomed && (binoculars_zoom || BinocularsActive());
  int32_t zoom_mode = kZoomKeep;
  if (g_zoomed)
  {
    zoom_mode = binoculars_zoom && REXCVAR_GET(pdz_tp_binoculars_fp) ? kZoomFirstPerson
                                                                      : REXCVAR_GET(pdz_tp_zoom_mode);
  }

  // Scoped artwork must be viewed from the native eye, not the lateral
  // "slide forward" camera. Scope observation is active for native artwork
  // too; camera FOV covers its initial/exit frames before the XUI heartbeat.
  PDZ_ClassicScopeState scope;
  const bool weapon_scope = is_player && !binoculars_zoom &&
      ((PDZ_ReadClassicScope(scope) && scope.aiming) || g_zoomed);
  if (weapon_scope) zoom_mode = kZoomFirstPerson;

  bool offset_written = false;
  static pdz::input::ShoulderTransition shoulder_transition;
  static uint32_t shoulder_mode=0;
  static std::chrono::steady_clock::time_point shoulder_time{};
  static pdz::input::ShoulderTransition scope_transition;
  static uint32_t scope_mode=0,scope_player=0;
  static std::chrono::steady_clock::time_point scope_time{};
  double scope_fraction=1;
  if(is_player && REXCVAR_GET(pdz_tp_enable)) {
    const auto now=std::chrono::steady_clock::now();
    if(scope_mode!=mode || scope_player!=target)scope_transition.Reset();
    const double dt=scope_time.time_since_epoch().count()?
        std::chrono::duration<double>(now-scope_time).count():0;
    scope_fraction=scope_transition.Update(zoom_mode==kZoomFirstPerson?0.:1.,dt);
    scope_mode=mode;scope_player=target;scope_time=now;
  } else if(is_player) {
    scope_transition.Reset();scope_mode=scope_player=0;scope_time={};
  }
  if (REXCVAR_GET(pdz_tp_enable) && is_player && scope_fraction>0 &&
      !DataThiefNeedsFirstPerson())
  {
    offset_written = true;
    double x = REXCVAR_GET(pdz_tp_offset_x);
    const auto shoulder_now=std::chrono::steady_clock::now();
    if(shoulder_mode!=mode)shoulder_transition.Reset();
    const double shoulder_dt=shoulder_time.time_since_epoch().count()?
        std::chrono::duration<double>(shoulder_now-shoulder_time).count():0;
    x=shoulder_transition.Update(x,shoulder_dt);
    shoulder_time=shoulder_now;shoulder_mode=mode;
    double y = REXCVAR_GET(pdz_tp_offset_y);
    double z = REXCVAR_GET(pdz_tp_offset_z);
    if (zoom_mode == kZoomForward && z < kLookDistance)
    {
      // The camera looks at eye + (0, 0, kLookDistance), not parallel to the
      // eyes. Slide it along that line (camera -> look point) up to eye depth:
      // the screen centre stays on the same ray, so the crosshair does not
      // jump, and the camera clears the character's head.
      const double k = kLookDistance / (kLookDistance - z);
      x *= k;
      y *= k;
      z = 0.0;
    }
    // Camera offsets only: never move Joanna, the native muzzle or shot ray.
    // Existing collision remains active throughout the short transition.
    x *= scope_fraction;y *= scope_fraction;z *= scope_fraction;
    StoreBeFloat(base, mode + kOffset + 0, static_cast<float>(x));
    StoreBeFloat(base, mode + kOffset + 4, static_cast<float>(y));
    StoreBeFloat(base, mode + kOffset + 8, static_cast<float>(z));
    g_offset_mode = mode;
  }
  else if (mode == g_offset_mode)
  {
    // The game only clears the offset at the end of a roll; do it ourselves.
    StoreBeFloat(base, mode + kOffset + 0, 0.0f);
    StoreBeFloat(base, mode + kOffset + 4, 0.0f);
    StoreBeFloat(base, mode + kOffset + 8, 0.0f);
    g_offset_mode = 0;
  }

  if (is_player && !offset_written)
  {
    shoulder_transition.Reset();shoulder_mode=0;shoulder_time={};
    g_fraction_valid = false;
  }
  g_collision_mode = offset_written ? mode : 0;
  __imp__sub_8220D9C8(ctx, base);
  g_collision_mode = 0;
  if(is_player) {
    std::lock_guard lock(clearance_mutex);
    clearance_eye.player=target;clearance_eye.active=offset_written;
    clearance_eye.time=SteadyMs();
    for(unsigned i=0;i<3;++i)clearance_eye.eye[i]=LoadBeFloat(base,mode+kEyePosition+4*i);
  }

  // While the binoculars hold a lock, they keep the target on the line through
  // the camera position along the eye direction, but the camera looks at the
  // eye-line point kLookDistance ahead, about 3.5 degrees off that line - a
  // third of the screen at binocular zoom. Look straight at the locked target
  // instead. Once a lock has happened during this zoom, keep looking along that
  // line after it ends, until the zoom ends, so releasing the lock does not
  // swing the view back.
  static bool lock_seen_this_zoom = false;
  if (!binoculars_zoom)
  {
    lock_seen_this_zoom = false;
  }
  if (!offset_written || !binoculars_zoom)
  {
    return;
  }

  if (const uint32_t locked = BinocularsLockTarget(base))
  {
    lock_seen_this_zoom = true;
    // Ignore a stale or garbage target: it must be ahead within binocular range.
    float distance2 = 0.0f;
    for (uint32_t axis = 0; axis < 12; axis += 4)
    {
      const float d =
          LoadBeFloat(base, locked + kTargetPosition + axis) - LoadBeFloat(base, mode + kCameraPosition + axis);
      distance2 += d * d;
    }
    if (distance2 > 100.0f * 100.0f && distance2 < 30000.0f * 30000.0f)
    {
      std::memcpy(base + mode + kLookPoint, base + locked + kTargetPosition, 12);
      return;
    }
  }

  if (lock_seen_this_zoom)
  {
    // Look point = camera + (look point - eye): parallel to the eye line.
    for (uint32_t axis = 0; axis < 12; axis += 4)
    {
      const float look = LoadBeFloat(base, mode + kCameraPosition + axis) +
                         LoadBeFloat(base, mode + kLookPoint + axis) -
                         LoadBeFloat(base, mode + kEyePosition + axis);
      StoreBeFloat(base, mode + kLookPoint + axis, look);
    }
  }
}

// sub_826285D0(from, to, out, f1 = max step): moves a vector towards another.
// The gameplay camera uses it on its camera vector (+172) towards the game's
// collision result, right before camera = eye (still at +4) + basis * vector.
REX_HOOK_RAW(sub_826285D0)
{
  const uint32_t mode = g_collision_mode;
  if (!mode || ctx.r3.u32 != mode + kCameraVector || ctx.r5.u32 != mode + kCameraVector)
  {
    __imp__sub_826285D0(ctx, base);
    return;
  }

  // The camera update's frame is still ctx.r1: this function is a leaf.
  float eye[3], offset[3], end[3];
  for (uint32_t i = 0; i < 3; ++i)
  {
    eye[i] = LoadBeFloat(base, mode + kCameraPosition + i * 4);
    offset[i] = LoadBeFloat(base, mode + kOffset + i * 4);
  }
  for (uint32_t i = 0; i < 3; ++i)
  {
    end[i] = eye[i];
    for (uint32_t axis = 0; axis < 3; ++axis)
    {
      end[i] += LoadBeFloat(base, ctx.r1.u32 + kCallerBasis + axis * 16 + i * 4) * offset[axis];
    }
  }

  const float free = FreeFraction(ctx, base, eye, end);
  const auto now = std::chrono::steady_clock::now();
  if (!g_fraction_valid || free < g_fraction)
  {
    g_fraction = free;
  }
  else
  {
    const double dt = std::min(0.1, std::chrono::duration<double>(now - g_fraction_time).count());
    g_fraction += (free - g_fraction) * static_cast<float>(1.0 - std::exp(-kEaseOutPerSecond * dt));
  }
  g_fraction_valid = true;
  g_fraction_time = now;

  for (uint32_t i = 0; i < 3; ++i)
  {
    StoreBeFloat(base, mode + kCameraVector + i * 4, offset[i] * g_fraction);
  }
}

// Data Thief (vtable 820CEB48) +4: hacking minigame update.
REX_HOOK_RAW(sub_82637BA0)
{
  g_data_thief_active_ms.store(SteadyMs(), std::memory_order_relaxed);
  __imp__sub_82637BA0(ctx, base);
}

// Binoculars (AudioScope, vtable 820CA998) +4: update.
REX_HOOK_RAW(sub_825B2190)
{
  g_binoculars_active_ms.store(SteadyMs(), std::memory_order_relaxed);
  g_binoculars_object.store(ctx.r3.u32, std::memory_order_relaxed);
  __imp__sub_825B2190(ctx, base);
}

namespace third_person
{
  void RegisterBinds()
  {
    rex::ui::RegisterBind("bind_third_person", "F7", "Toggle third-person camera", []
                          {
      // Held keys auto-repeat; only a press after a quiet gap counts.
      static auto last_event = std::chrono::steady_clock::time_point{};
      const auto now = std::chrono::steady_clock::now();
      const bool fresh_press = now - last_event > std::chrono::milliseconds(300);
      last_event = now;
      if (fresh_press)
      {
        REXCVAR_SET(pdz_tp_enable, !REXCVAR_GET(pdz_tp_enable));
      } });
  }
} // namespace third_person
