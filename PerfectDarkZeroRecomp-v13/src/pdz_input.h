#pragma once
#include "pdz_input_math.h"
#include "pdz_overlay_controller.h"
#include <toml++/toml.hpp>
void PDZ_InitializeInput();
pdz::ui::OverlayPad PDZ_OverlayController();
bool PDZ_ConsumeOverlayToggle();
bool PDZ_ConsumeRenderToggle();
bool PDZ_ConsumeShoulderToggle();
bool PDZ_ConsumePerspectiveToggle();
void PDZ_SetOverlayControllerCapture(bool captured);
bool PDZ_BindingCaptureActive();
bool PDZ_GameMenuActive();
bool PDZ_LocktopusInputActive();
enum class PDZ_InputPage { KeyboardMouse, Controller, Menus, Gyro, Experimental };
void PDZ_DrawInputTab(bool& changed,PDZ_InputPage page);
void PDZ_SaveInputSettings(toml::table& values);
void PDZ_DefaultPresetBindings(toml::table& values);
pdz::input::Point PDZ_ControllerTurn(bool aiming);
pdz::input::Point PDZ_GyroTurn(bool aiming);
void PDZ_QueueZoomWheel(float amount);
