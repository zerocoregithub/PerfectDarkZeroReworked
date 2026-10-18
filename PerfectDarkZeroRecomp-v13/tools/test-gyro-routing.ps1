$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot
$input=Get-Content "$repo/src/pdz_input.cpp" -Raw
$qol=Get-Content "$repo/src/pdz_qol.cpp" -Raw
$overlay=Get-Content "$repo/src/pdz_overlay.cpp" -Raw
$sdl=Get-Content "$repo/../rexglue-sdk-v13/src/input/sdl/sdl_input_driver.cpp" -Raw
foreach($text in @('if(user!=0||info.synthetic) return;','system->GetDeviceMotion(info.id,motion)',
 'sensor.bias[i]=sensor.sum[i]/sensor.samples','PDZ_GameMenuActive()',
 'pdz::input::masks[clutch]','PDZ_InputPage::Gyro')){if(!$input.Contains($text)){throw "Gyro input guard missing: $text"}}
foreach($text in @('if(free_gesture && !paused)',
 'PDZ_GyroTurn(aiming)','gyro_turn.x/zoom_factor')){if(!$qol.Contains($text)){throw "Aim routing missing: $text"}}
if(!$overlay.Contains('PDZ_DrawInputTab(gyro_changed,PDZ_InputPage::Gyro)')){throw 'Dedicated gyro page missing'}
$gyroTabStart=$overlay.IndexOf('if(begin_tab("Gyro"))')
$gyroTabEnd=$overlay.IndexOf('if(begin_tab("Menus"))',$gyroTabStart)
if($gyroTabStart -lt 0 -or $gyroTabEnd -le $gyroTabStart -or $gyroTabStart -lt $overlay.IndexOf('if(begin_tab("Controller"))') -or !$overlay.Substring($gyroTabStart,$gyroTabEnd-$gyroTabStart).Contains('PDZ_InputPage::Gyro')){throw 'Gyro tab not beside Controller'}
$freeAim=$qol.IndexOf('if(free_gesture && !paused)')
$stickAddition=$qol.IndexOf('degrees_x+=float(pad_turn.x);degrees_y+=float(pad_turn.y);',$freeAim)
if($freeAim -lt 0 -or $stickAddition -lt $qol.IndexOf('free_aim_state.Update(',$freeAim)){throw 'Stick must be added after mouse/gyro free-aim processing'}
if($qol.Contains('if(stick_free&&free_gesture)')){throw 'Removed experimental stick free-aim route restored'}
foreach($text in @('system->GetDeviceNativeCapabilities(info.id)','capabilities.gyro','capabilities.steam_controller','page==PDZ_InputPage::Controller&&visible_extras')){if(!$input.Contains($text)){throw "Detection-aware UI missing: $text"}}
foreach($text in @('SDL_GamepadHasButton','SDL_GamepadHasSensor','SDL_GetNumGamepadTouchpads')){if(!$sdl.Contains($text)){throw "Native capability discovery missing: $text"}}
if($input.Contains('Native gyro (Experimental')){throw 'Gyro still labelled experimental'}
foreach($text in @('SDL_EVENT_GAMEPAD_SENSOR_UPDATE','SDL_SENSOR_GYRO','SDL_SetGamepadSensorEnabled',
 'SDL_GetTicksNS() - controller->motion.timestamp_ns <= 100000000ULL','SDL_HINT_JOYSTICK_HIDAPI_STEAM')){
 if(!$sdl.Contains($text)){throw "Native sensor path missing: $text"}
}
Write-Output 'PASS: physical player-one sensor routing, calibration, stale/focus gates, dedicated gyro tab and detected controller controls.'
foreach($text in @('pdz_gyro_space','pdz_gyro_enable_input','system->GetDeviceExtraButtons(info.id)',
 'sensor.orientation.Reset()','sample.accel_timestamp_ns','space?0:',
 'selected->extra_buttons','pdz_extra_buttons','extra_flags')){
 if(!$input.Contains($text)){throw "Extended gyro route missing: $text"}
}
foreach($text in @('SDL_SENSOR_ACCEL','sample.accel_timestamp_ns = event.gsensor.timestamp',
 'SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN','SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1',
 'return; // Keep extended controls independent')){
 if(!$sdl.Contains($text)){throw "Extended SDL route missing: $text"}
}
Write-Output 'PASS: gyro spaces, fresh accelerometer guard, extended button separation and touch/hold gating.'
$gyroStart=$input.IndexOf('pdz::input::Point PDZ_GyroTurn(')
$stickStart=$input.IndexOf('pdz::input::Point PDZ_ControllerTurn(')
$gyroBody=$input.Substring($gyroStart,$stickStart-$gyroStart)
if($gyroBody.Contains('pdz_stick_') -or $gyroBody.Contains('StickExponent')){throw 'Stick curve/settings leaked into gyro route'}
if(!$input.Contains('StickExponent(rex::cvar::Query<std::string>("pdz_stick_curve"))') -or !$input.Contains('StickExponent(current)')){throw 'Runtime/preview curve mapping absent'}
Write-Output 'PASS: gyro tuning independent of right-stick curve and sensitivity.'
