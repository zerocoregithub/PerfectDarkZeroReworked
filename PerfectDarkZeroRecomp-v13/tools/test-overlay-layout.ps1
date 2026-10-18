$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$overlay=Get-Content (Join-Path $repo 'src/pdz_overlay.cpp') -Raw
$input=Get-Content (Join-Path $repo 'src/pdz_input.cpp') -Raw
$mouse=Get-Content (Join-Path $repo 'src/pdz_ui_mouse.h') -Raw
$menu=$overlay.Substring($overlay.IndexOf('void PDZOverlayDialog::OnDraw'),$overlay.IndexOf('const bool capture_mouse =')-$overlay.IndexOf('void PDZOverlayDialog::OnDraw'))
$tabs=@('Presets','Movement','Weapons','Aiming','HUD','Display','Mouse/keys','Controller','Gyro','Menus','Experimental','System')
foreach($tab in $tabs){
  if([regex]::Matches($menu,[regex]::Escape('begin_tab("'+$tab+'")')).Count -ne 1){throw ('Missing/duplicate tab: '+$tab)}
}
if([regex]::Matches($menu,'if\(begin_tab\(').Count -ne $tabs.Count){throw 'Unexpected tab count'}
foreach($retired in @('Custom crosshair','Classic HUD design preview','Dab-inspired','pdz_crosshair_style','pdz_custom_crosshair')){
  if($menu.Contains($retired)){throw ('Retired menu control remains: '+$retired)}
}
foreach($setting in @('pdz_crosshair_size','pdz_crosshair_gap','pdz_crosshair_thickness','pdz_crosshair_outline',
                       'pdz_crosshair_red','pdz_crosshair_green','pdz_crosshair_blue','pdz_crosshair_opacity')){
  if(!$menu.Contains($setting)){throw ('Shared reticle control removed: '+$setting)}
}
if(!$menu.Contains('Checkbox("Enhanced dodge roll"') -or $menu.Contains('pdz_dodge_roll_strength')){throw 'Fixed regular dodge toggle missing or retired slider remains'}
foreach($page in @('KeyboardMouse','Controller','Menus')){
  if(!$menu.Contains('PDZ_DrawInputTab(input_changed,PDZ_InputPage::'+$page+')') -or
     !$input.Contains('page==PDZ_InputPage::'+$page)){throw ('Input page routing missing: '+$page)}
}
if(!$input.Contains('if(previous_page!=page){listening=-1;previous_page=page;}')){throw 'Binding capture page-change cancellation absent'}
if(!$mouse.Contains('window->RootWindow') -or !$mouse.Contains('Perfect Dark Zero settings##pdz')){throw 'Own child-window mouse capture exclusion absent'}
foreach($fragment in @('const auto scope_color=read_reticle_color();','const auto reticle_color=read_reticle_color();',
                       'scope.aiming,cursor,scope_color','160*scope_color.a/255')){
  if(!$overlay.Contains($fragment)){throw ('Shared scope/reticle color or opacity wiring absent: '+$fragment)}
}
$scopeStart=$overlay.IndexOf('PDZ_ClassicScopeState scope;')
$scopeEnd=$overlay.IndexOf('const bool classic_reticle=', $scopeStart)
if($overlay.Substring($scopeStart,$scopeEnd-$scopeStart).Contains('IM_COL32(0,255,0,255)')){throw 'Fixed green/full-opacity scope drawing remains'}
foreach($fragment in @('pdz::ui::BeginSettingsPage(label)','ImGuiTabBarFlags_FittingPolicyScroll','ImGui::PushTextWrapPos(0.f)','ImGui::PopTextWrapPos()')){
  if(!$menu.Contains($fragment)){throw ('Scrollable page/footer layout missing: '+$fragment)}
}
if($menu.Contains('Auto-aim while scoped') -or $overlay.Contains('Query<bool>("pdz_auto_aim_scoped")')){throw 'Scoped assist option remains'}
$experimentStart=$menu.IndexOf('if(begin_tab("Experimental"))')
$experimentEnd=$menu.IndexOf('if(begin_tab("System"))')
foreach($flag in @('pdz_stylized_lighting','pdz_cover_camera_stabilization','pdz_hide_unarmed_hands','pdz_weapon_accuracy','pdz_auto_aim_range_boost')){
  if(!$menu.Substring($experimentStart,$experimentEnd-$experimentStart).Contains($flag)){throw ('Experimental feature misplaced: '+$flag)}
}
foreach($completed in @('pdz_dodge_roll','pdz_auto_aim_retention')){
  if($menu.Substring($experimentStart,$experimentEnd-$experimentStart).Contains($completed)){throw ('Completed/retired experimental control remains: '+$completed)}
}
if(!$overlay.Contains('"pdz_cover_camera_stabilization"') -or !$menu.Contains('Checkbox("Stable cover camera (test)"')){throw 'Reversible cover camera test toggle missing'}
if(!$menu.Contains('if(settings_dirty_&&!ImGui::IsAnyItemActive()') -or $menu.Contains('Button("Save settings")')){throw 'Autosave/footer not wired'}
foreach($fragment in @('const bool preview_classic_reticle=show_menu&&classic_reticle;',
    'if (!preview_classic_reticle&&(capture_mouse || !PDZ_ShouldDrawCustomCrosshair())) return;',
    'if(!preview_classic_reticle) PDZ_ReadVisualCrosshair(aim_x,aim_y);')){
  if(!$overlay.Contains($fragment)){throw ('Classic F8 reticle preview guard missing: '+$fragment)}
}
foreach($fragment in @('PDZ_DefaultPresetBindings(values);','Preset::GameCube,"gamecube"',
 'for(int slot=1;slot<=3;++slot)','"user-preset-"+std::to_string(slot)+".toml"',
 'SaveEnhancementFile(path,CaptureSettings(),error)','ApplySettings(toml::parse_file(path.string()),error)',
 'ImGui::BeginPopupModal("Overwrite preset?"','!known.contains(name)')){
 if(!$overlay.Contains($fragment)){throw ('Preset defaults/independent slot protection absent: '+$fragment)}
}
foreach($fragment in @('values.insert_or_assign(b.flag,b.modern);',
 'values.insert_or_assign(pad_flags[i],i);','values.insert_or_assign(menu_pad_flags[i],i);')){
 if(!$input.Contains($fragment)){throw ('Default input preset missing: '+$fragment)}
}
Write-Output 'PASS: menu, autosave, F8 crosshair preview, default bindings and independent save/load slots.'
if(!$menu.Contains('pdz::ui::BeginSettingsPage(label)')){throw 'Per-page bounded scrolling absent'}
if($input.IndexOf('BeginCombo("Gyro space"') -gt $input.IndexOf('slider("Gyro sensitivity"')){throw 'Gyro space buried below tuning sliders'}
if(!$input.Contains('const char* axes[]={"Yaw","Roll","Yaw + Roll"}') -or $input.IndexOf('Combo("Local horizontal axis"') -gt $input.IndexOf('slider("Gyro sensitivity"')){throw 'Local gyro axes not exposed beside space choice'}
foreach($name in @('Touchpad Click Left','Touchpad Click Right','Left Stick Touch','Right Stick Touch','Left Grip Touch','Right Grip Touch')){if(!$input.Contains($name)){throw "Missing readable controller name: $name"}}
