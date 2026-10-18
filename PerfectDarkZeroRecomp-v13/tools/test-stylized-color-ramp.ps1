$ErrorActionPreference='Stop'
function Ramp([double]$x){
  $x=[Math]::Clamp($x,0.0,1.0)
  .5*([Math]::Clamp(($x-.22)/.06,0.0,1.0)+[Math]::Clamp(($x-.72)/.06,0.0,1.0))
}
foreach($pair in @(@(0,0),@(.2,0),@(.25,.25),@(.3,.5),@(.7,.5),@(.75,.75),@(.8,1),@(1,1))){
  if([Math]::Abs((Ramp $pair[0])-$pair[1]) -gt .00001){throw 'Band plateau/transition mismatch'}
}
$prev=-1.0
for($i=0;$i -le 10000;$i++){$v=Ramp ($i/10000.0);if($v -lt $prev -or $v-$prev -gt .001){if($prev -ge 0){throw 'Nonmonotonic/discontinuous ramp'}};$prev=$v}
foreach($rgb in @(@(0,0,0),@(.2,.2,.2),@(1,1,1),@(.8,.15,.1))){
  $l=$rgb[0]*.2126+$rgb[1]*.7152+$rgb[2]*.0722
  $out=@($rgb|ForEach-Object {[Math]::Max(0.0,($_-$l)*1.12+$l)})
  if($rgb[0] -eq $rgb[1] -and $rgb[1] -eq $rgb[2]){for($i=0;$i -lt 3;$i++){if([Math]::Abs($out[$i]-$rgb[$i]) -gt .00001){throw 'Neutral/black changed'}}}
  else{if($out[0]-$out[1] -le $rgb[0]-$rgb[1]){throw 'Chroma not increased'}}
}
'PASS: black/neutral preservation, chroma increase, monotonic continuous three-band ramp and narrow transition limits'
