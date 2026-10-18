$ErrorActionPreference='Stop'
$source=Get-Content (Join-Path $PSScriptRoot '../src/pdz_overlay.cpp') -Raw
# A globally balanced count can hide a missing close in one tab and a stray
# close outside it. Check each independently executable page, plus the remainder.
$tabs=[regex]::Matches($source,'if\(begin_tab\("([^"\r\n]+)"\)\)\s*\{')
if($tabs.Count -ne 12){throw 'Unexpected tab structure; review test boundaries'}
$end=$source.IndexOf('ImGui::EndTabBar();',$tabs[$tabs.Count-1].Index)
$regions=[Collections.Generic.List[object]]::new()
for($i=0;$i -lt $tabs.Count;++$i){
 $stop=if($i+1 -lt $tabs.Count){$tabs[$i+1].Index}else{$end}
 $regions.Add([pscustomobject]@{Name=$tabs[$i].Groups[1].Value;Text=$source.Substring($tabs[$i].Index,$stop-$tabs[$i].Index)})
}
$regions.Add([pscustomobject]@{Name='Outside tab pages';Text=$source.Substring(0,$tabs[0].Index)+$source.Substring($end)})
foreach($region in $regions){
 $depth=0
 foreach($call in [regex]::Matches($region.Text,'ImGui::(BeginDisabled|EndDisabled)\s*\(')){
  if($call.Groups[1].Value -eq 'BeginDisabled'){++$depth}else{--$depth}
  if($depth -lt 0){throw "Unmatched EndDisabled in $($region.Name)"}
 }
 if($depth -ne 0){throw "Unclosed BeginDisabled in $($region.Name)"}
}
'PASS: all 12 tabs and parent overlay independently balance disabled-control scopes'
