$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot
$sdl=Get-Content "$repo/../rexglue-sdk-v13/src/input/sdl/sdl_input_driver.cpp" -Raw
$wrapper=Get-Content "$PSScriptRoot/proton-native-input.sh" -Raw
$native=$sdl.IndexOf('if (NativeSteamRequested()) {')
$init=$sdl.IndexOf('if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))',$native)
if($native -lt 0 -or $init -le $native){throw 'Native SDL setup guard missing'}
$setup=$sdl.Substring($native,$init-$native)
foreach($hint in @('SDL_HINT_JOYSTICK_HIDAPI_STEAM,','SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK,')) {
    if(!$setup.Contains($hint)){throw "Pre-init hint missing: $hint"}
}
foreach($marker in @('PROTON_ENABLE_HIDRAW','SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT',
    'SDL_HIDAPI_IGNORE_DEVICES','/sys/class/hidraw/hidraw*/device/uevent',
    '1101|1102|1105|1106|1142|1201|1202|1205|1302|1303|1304|1305',
    'exec "$@"')) {
    if(!$wrapper.Contains($marker)){throw "Host wrapper marker missing: $marker"}
}
if($wrapper.Contains("`r")){throw 'Wrapper must use Linux LF line endings'}
if($wrapper -match '(?m)^\s*(sudo|chmod|chown|systemctl|rm)\s'){throw 'Unexpected system mutation'}
Write-Output 'PASS: structural pre-init opt-in, bounded host routing, preserved argv, LF and no system mutation.'
Write-Output 'Not a Bash execution test, device test or Proton compatibility acceptance.'
