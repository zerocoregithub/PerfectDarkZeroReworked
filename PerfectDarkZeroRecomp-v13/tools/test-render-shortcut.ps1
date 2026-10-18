$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$source=Get-Content (Join-Path $repo 'src/pdz_overlay.cpp') -Raw
foreach($fragment in @('!io.WantTextInput','!io.WantCaptureKeyboard','!ImGui::IsAnyItemActive()',
                       '!io.KeyCtrl','!io.KeyAlt','!io.KeySuper',
                       'ImGui::IsKeyPressed(ImGuiKey_Semicolon,false)',
                       'const bool enabled=!rex::cvar::Query<bool>("pdz_stylized_lighting");',
                       'set_bool("pdz_stylized_lighting",enabled);',
                       'PDZ stylized lighting shortcut: {}')) {
  if(!$source.Contains($fragment)){throw ('Shortcut regression: '+$fragment)}
}
if($source.IndexOf('ImGui::IsKeyPressed(ImGuiKey_Semicolon,false)') -gt $source.IndexOf('if (show_menu)')) {
  throw 'Shortcut restricted to open settings menu'
}
'PASS: semicolon edge-only toggle, typing/capture/modifier guards, save-dirty path and diagnostic log'
