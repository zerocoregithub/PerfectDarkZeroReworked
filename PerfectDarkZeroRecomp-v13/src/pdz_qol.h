#pragma once

// Host callbacks used by generated TU0 adapters, not a stable SDK/plugin ABI.
// uint32_t parameters are guest addresses/values; base is the runtime's guest
// memory mapping. Do not replace these with host pointers or reorder hooks.
// TraceCoverCamera observes gameplay state even in quiet builds; only explicitly
// gated diagnostic declarations below may become no-ops.

#include <cstdint>
struct PPCContext;
void PDZ_ConstrainThirdPersonTarget(PPCContext& ctx,uint8_t* base,uint32_t player,uint32_t target);
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
// Read-only, opt-in, bounded cover aim-order diagnostic.
void PDZ_TraceCoverAimOrder(uint8_t* base,uint32_t player,unsigned stage,
                           uint32_t first,uint32_t second,unsigned count,uint32_t detail);
#else
inline void PDZ_TraceCoverAimOrder(uint8_t*,uint32_t,unsigned,uint32_t,uint32_t,unsigned,uint32_t) {}
#endif
void PDZ_RecordCoverTarget(uint8_t* base,uint32_t target);
struct PDZ_ThirdPersonImpact {double x=0,y=0;bool blocked=false;};
bool PDZ_ReadThirdPersonImpact(PDZ_ThirdPersonImpact& impact);
// Gate only the DoorProp update's timed-close call, never shared close helpers.
bool PDZ_AllowTimedDoorClose(uint8_t* base, uint32_t door);
void PDZ_RecordWeaponInput(uint8_t* base, uint32_t state, uint32_t user, uint32_t result);
double PDZ_DpadZoomPosition(uint8_t* base, uint32_t player, double original,
                            double seconds, bool allowed);

void PDZ_AdjustDodgeRollDelta(uint8_t* base, uint32_t player, uint32_t config,
                             double seconds, double& x, double& z);
double PDZ_AdjustDodgeRollVelocity(uint8_t* base, uint32_t player, uint32_t config,
                                  double seconds, double& x, double& z);
void PDZ_ObserveDodgeRoll(uint8_t* base, uint32_t player, uint32_t config, double seconds);
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
bool PDZ_TraceDodgeRoute(uint8_t* base, uint32_t player, uint32_t config,
                         double seconds, unsigned stage);
#else
inline bool PDZ_TraceDodgeRoute(uint8_t*,uint32_t,uint32_t,double,unsigned) { return false; }
#endif

double PDZ_ClassicNpcHealthLoss(uint8_t* base, uint32_t character, double original);

double PDZ_WeaponActionRate(uint8_t* base, uint32_t weapon_state,
                            uint32_t requested_action, double original);

double PDZ_AdjustMovementRetention(uint8_t* base, uint32_t player,
                                   double original, uint32_t axis, double seconds,
                                   double old, double target);
double PDZ_ClassicTargetScale(uint8_t* base, uint32_t player, uint32_t input,
                              double seconds, uint32_t axis);
double PDZ_ClassicEquipmentFactor(uint8_t* base, uint32_t player,
                                  uint32_t caller, double original);

void PDZ_ApplyDirectMouse(uint8_t* base, uint32_t horizontal_address,
                          uint32_t vertical_address, uint32_t input_slot);
void PDZ_TraceCoverCamera(uint8_t* base, uint32_t player, uint32_t camera,
                          uint32_t branch_state, uint32_t phase);
// Legacy hook name above: paired cover observation is required by gameplay;
// only its extra snapshot fields/logging are developer diagnostics.

#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
void PDZ_TraceStickCall(uint32_t input_slot_address, uint32_t link_register,
                        uint32_t horizontal_address, uint32_t vertical_address);
void PDZ_TraceCameraRead(uint32_t link_register, uint32_t input_state_address);
void PDZ_TraceVisionState(uint8_t* base, uint32_t state_address);
void PDZ_TraceRcpSubmission(uint8_t* base, uint32_t descriptor, uint32_t stage);
#else
inline void PDZ_TraceStickCall(uint32_t,uint32_t,uint32_t,uint32_t) {}
inline void PDZ_TraceCameraRead(uint32_t,uint32_t) {}
inline void PDZ_TraceVisionState(uint8_t*,uint32_t) {}
inline void PDZ_TraceRcpSubmission(uint8_t*,uint32_t,uint32_t) {}
#endif
void PDZ_AdjustFinalCoverView(uint8_t* base, uint32_t matrix_address,
                              uint32_t caller);
void PDZ_AdjustCoverTargetRay(uint8_t* base,uint32_t player,uint32_t native_origin,
                             uint32_t ray_start,uint32_t ray_end,
                             double& start_x,double& start_y,double& start_z);

// Offsets the gun-only visual values used by the first-person sway accessors.
// This does not alter the camera angles or the point at which shots land.
void PDZ_AdjustGunVisual(double& value, uint32_t axis);
void PDZ_ApplyClassicWeaponSway(uint8_t* base, uint32_t player,
                              double& vertical, double& horizontal);
void PDZ_AdjustWeaponBob(uint8_t* base,uint32_t player,uint32_t parameters,
                         double& x,double& y);
void PDZ_ObserveUnarmedAction(uint8_t* base,uint32_t state,uint32_t action,uint32_t success);
void PDZ_AdjustUnarmedViewmodel(uint8_t* base,uint32_t player,uint32_t item,uint32_t matrix);
uint32_t PDZ_IdleFistVisibility(uint8_t* base,uint32_t player,uint32_t native_mask);
void PDZ_ApplySwayAim(uint8_t* base, uint32_t player, uint32_t camera,
                      uint32_t screen_point, double native_x, double native_y,
                      double zoom);
bool PDZ_ReadSwayCrosshair(double& x, double& y);
bool PDZ_ReadVisualCrosshair(double& x, double& y);
struct PDZ_ClassicScopeState { float current_zoom=1, selected_zoom=1; bool aiming=false; };
bool PDZ_ReadClassicScope(PDZ_ClassicScopeState& state);
void PDZ_ObserveScopeScene(uint8_t* base, uint32_t resource, uint32_t output);
uint32_t PDZ_ScopeAllowed(uint8_t* base, uint32_t player, uint32_t original);
void PDZ_ApplyAutoAimSetting(uint8_t* base, uint32_t player);
void PDZ_AdjustAimRetentionBounds(uint8_t* base,uint32_t assist,uint32_t candidate,
                                 uint32_t rect,double x,double y,double scale_x,double scale_y);
void PDZ_AdjustWeaponSpread(uint8_t* base,uint32_t player,uint32_t definition,double& spread);
uint32_t PDZ_AutoAimApplicationMode(uint8_t* base, uint32_t player, uint32_t original,
                                   double& attraction_strength, uint32_t assist);
void PDZ_ObserveAutoAim(uint8_t* base, uint32_t assist, bool applied);

void PDZ_AdjustProjectionFrustum(double& left, double& right,
                                 double& bottom, double& top,
                                 double near_plane, double far_plane,
                                 uint32_t caller);

// Applies a changed horizontal-FOV setting to both the shared camera default
// and the currently active camera while retaining the current zoom ratio.
void PDZ_UpdateLiveFov();
// Replace only the active local puzzle's analog samples. Unlike a diagnostic,
// this updates x/y and the puzzle-input lease; it does not write shared pad state.
void PDZ_ApplyLocktopusMouse(uint8_t* base,uint32_t puzzle,double& x,double& y,double seconds=0);
