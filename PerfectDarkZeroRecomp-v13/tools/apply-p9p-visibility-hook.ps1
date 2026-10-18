param([Parameter(Mandatory)][string]$GeneratedDir)
$ErrorActionPreference='Stop'
foreach($id in @(69,142,174)) {
 $file=Join-Path $GeneratedDir "perfectdarkzerorecomp_recomp.$id.cpp"
 $before=[IO.File]::ReadAllText($file);$text=$before.Replace("`r`n","`n")
 $include="#include `"perfectdarkzerorecomp_funcs.$id.h`""
 $model='#include "../../src/pdz_p9p_model.h"'
 $text=$text.Replace('#include "../../src/pdz_p9p_trace.h"',$model)
 if(!$text.Contains($model)) {
  if(!$text.Contains($include)){throw 'P9P include marker missing'}
  $text=$text.Replace($include,"$include`n$model")
 }
 $text=$text -replace '(?m)^\s*PDZ_P9PVisibilityScope p9p_visibility[^\r\n]*\n',''
 $text=$text -replace '(?m)^\s*PDZ_P9POwnerTraceScope p9p_owner_trace[^\r\n]*\n',''
 $text=$text -replace '(?m)^\s*PDZ_P9PTraceScope p9p_trace[^\r\n]*\n',''
 $text=$text -replace '(?m)^\s*PDZ_TraceP9PGroup[^\r\n]*\n',''
 if($id -eq 69) {
  $entry="DEFINE_REX_FUNC(sub_827017F8) {`n`tREX_FUNC_PROLOGUE();"
  $scope='PDZ_P9PModelScope p9p_model_scope(base,ctx.r3.u32,ctx.r30.u32,uint32_t(ctx.lr));'
  if(!$text.Contains($scope)) {
   if(!$text.Contains($entry)){throw 'P9P entry missing'}
   $text=$text.Replace($entry,"$entry`n`t$scope")
  }
 } elseif($id -eq 142) {
  $scope='PDZ_P9PModelScope p9p_model_scope(base,0,0,0);'
  if(!$text.Contains($scope)) {
   $call="`tsub_82700F80(ctx, base);"
   $start=$text.IndexOf('ctx.lr = 0x827018C4;')
   if($start -lt 0){throw 'Alternative P9P route missing'}
   $pos=$text.IndexOf($call,$start)
   if($pos -lt 0 -or $pos-$start -gt 100){throw 'Alternative interpreter call missing'}
   $text=$text.Substring(0,$pos)+"`t{ $scope`n$call`n`t}"+$text.Substring($pos+$call.Length)
  }
 } else {
  $start=$text.IndexOf('loc_827010A8:');$end=$text.IndexOf('loc_827010DC:',$start)
  if($start -lt 0 -or $end -le $start){throw 'P9P native group block missing'}
  $block=$text.Substring($start,$end-$start)
  $old='if (ctx.cr0.eq) goto loc_827017EC;'
  $new='if (ctx.cr0.eq || PDZ_P9PGroupHidden(REX_LOAD_U32(ctx.r31.u32+12u))) goto loc_827017EC;'
  if(!$block.Contains($new)) {
   if(!$block.Contains($old)){throw 'Native visibility decision missing'}
   $text=$text.Substring(0,$start)+$block.Replace($old,$new)+$text.Substring($end)
  }
 }
 if($text -match 'PDZ_P9PVisibilityScope p9p_visibility|PDZ_P9PTraceScope|PDZ_TraceP9PGroup|PDZ_P9POwnerTraceScope'){throw 'Obsolete P9P hook remains'}
 if($text -ne $before.Replace("`r`n","`n")){[IO.File]::WriteAllText($file,$text)}
}
