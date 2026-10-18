$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$input=Get-Content "$repo/src/pdz_input.cpp" -Raw
$overlay=Get-Content "$repo/src/pdz_overlay.cpp" -Raw
$sdl=Get-Content "$repo/../rexglue-sdk-v13/src/input/sdl/sdl_input_driver.cpp" -Raw
$drawer=Get-Content "$repo/../rexglue-sdk-v13/src/ui/imgui_drawer.cpp" -Raw
$system=Get-Content "$repo/../rexglue-sdk-v13/src/input/input_system.cpp" -Raw
if(!$system.Contains('dynamic_cast<sdl::SDLInputDriver*>') -or !$system.Contains('window_->HasFocus()')){throw 'Physical menu polling incorrectly depends on mouse capture or ignores window focus'}
foreach($method in @('GetState','GetStateForUI','GetCapabilities','SetState','GetKeystroke','GetDeviceExtraButtons','GetDeviceMotion','GetDeviceNativeCapabilities','RemoveUIInputBlocker')) {
  $match=[regex]::Match($system,"InputSystem::$method\([^{}]*\)\s*\{\s*std::lock_guard lock\(state_mutex_\);")
  if(!$match.Success){throw "Unserialized controller entry: $method"}
}
foreach($fragment in @('overlay.shortcuts.Update(physical_buttons','overlay.pad={physical_buttons','overlay.drain','state.gamepad={};')){if(!$input.Contains($fragment)){throw "Physical navigation/capture missing: $fragment"}}
foreach($fragment in @('void PDZOverlayDialog::OnPrepareFrame','input->AddUIInputBlocker()','input->RemoveUIInputBlocker()','input->GetStateForUI(0','pdz::ui::FeedOverlayPad(io,pad,show_menu_)','ImGuiTabItemFlags_SetSelected','pdz_overlay_scale','ImGuiCol_NavCursor','cancel_owned_','PDZ_ConsumeRenderToggle()')){if(!$overlay.Contains($fragment)){throw "Controller menu route missing: $fragment"}}
if($drawer.IndexOf('dialog->PrepareFrame(io)') -gt $drawer.IndexOf('ImGui::NewFrame()')){throw 'Navigation fed after NewFrame'}
if(!$sdl.Contains('SDL_GAMEPAD_BUTTON_MISC1') -or !$sdl.Contains('uint64_t(1) << 28') -or !$sdl.Contains('Native Quick Access is not the guide button')){throw 'QAM still aliased to Guide'}
$keystroke=$system.Substring($system.IndexOf('X_RESULT InputSystem::GetKeystroke('))
if(!$keystroke.Contains('ui_input_blockers_.load() > 0')){throw 'Keystrokes leak through captured menu'}
'PASS: pre-frame physical controller input, fixed navigation, QAM separation, tab switching, scaling and gameplay blocking.'
