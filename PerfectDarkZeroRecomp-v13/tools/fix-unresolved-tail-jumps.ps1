# Generated TU0 compatibility and feature-hook adapter (historical filename).
# It now owns much more than tail-jump repair: HUD, input, movement, targeting
# and camera call sites. Do not delete a block just because its name says trace:
# some observers feed gameplay state even in non-diagnostic builds.
# Keep the supported XEX, legacy generator and partition seed matched. Generated
# output is disposable, but this adapter and its ordering are versioned inputs.
# The focused fixture checks repeated application and selected fresh insertions;
# it is not a proof that arbitrary newer compiler output is compatible.
#
# Auto-reapplies the "cross-function tail-jump" workaround
# Classic HUD attribution observes the original sprite batch without hiding it.
# Local-player walking blend hooks are reapplied at the end of this script.
# Weapon action rate hooks preserve pre-remapping GunAnim IDs.
# after every `rexglue codegen` run, since codegen regenerates these files
# from scratch and doesn't preserve hand edits. Safe to re-run: a file with
# no remaining broken gotos is left untouched.
#
# What it looks for: a `goto loc_XXXXXXXX;` whose target label isn't declared
# anywhere in the same generated .cpp file (PPC compilers sometimes emit a
# shared tail block split across two functions by rexglue's Discover/Merge
# phases), and conditional branches that codegen marked unresolved even though
# their target label is present. Missing targets become logged early returns;
# false unresolved branches are restored as ordinary gotos.
param(
    [string]$GeneratedDir = (Join-Path $PSScriptRoot "..\generated\default")
)

$ErrorActionPreference = 'Stop'
$GeneratedDir = (Resolve-Path -LiteralPath $GeneratedDir).Path
& (Join-Path $PSScriptRoot 'patch-generated-runtime.ps1') -GeneratedDir $GeneratedDir

# Optional cosmetic model visibility hook; separate idempotent adapter avoids
# regenerating unrelated accepted game code for a local candidate.
& (Join-Path $PSScriptRoot 'apply-p9p-visibility-hook.ps1') -GeneratedDir $GeneratedDir

# Locktopus only: replace its local analog samples, never shared input state.
$lockFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.169.cpp'
$lockOld = [IO.File]::ReadAllText($lockFile)
$lockText = $lockOld.Replace("`r`n", "`n")
$lockInclude = '#include "perfectdarkzerorecomp_funcs.169.h"'
if (!$lockText.Contains('#include "../../src/pdz_qol.h"')) {
    if (!$lockText.Contains($lockInclude)) { throw 'Locktopus include missing' }
    $lockText=$lockText.Replace($lockInclude,"$lockInclude`n#include `"../../src/pdz_qol.h`"")
}
$lockMarker="loc_826398B8:`n"
$lockEntry="DEFINE_REX_FUNC(sub_82639498) {`n`tREX_FUNC_PROLOGUE();"
$lockReset="$lockEntry`n`tif (REX_LOAD_U32(ctx.r3.u32 + 32) != 1) {`n`t`tdouble pdz_x=0, pdz_y=0;`n`t`tPDZ_ApplyLocktopusMouse(base, ctx.r3.u32, pdz_x, pdz_y);`n`t}"
if (!$lockText.Contains($lockReset)) {
    if (!$lockText.Contains($lockEntry)) { throw 'Locktopus update entry missing' }
    $lockText=$lockText.Replace($lockEntry,$lockReset)
}
$lockText=$lockText.Replace('PDZ_ApplyLocktopusMouse(base, ctx.r31.u32, ctx.f13.f64, ctx.f12.f64);','PDZ_ApplyLocktopusMouse(base, ctx.r31.u32, ctx.f13.f64, ctx.f12.f64, ctx.f29.f64);')
$lockPatch="$lockMarker`tPDZ_ApplyLocktopusMouse(base, ctx.r31.u32, ctx.f13.f64, ctx.f12.f64, ctx.f29.f64);`n`ttemp.f32 = float(ctx.f13.f64);`n`tREX_STORE_U32(ctx.r31.u32 + 124, temp.u32);`n"
if (!$lockText.Contains($lockPatch)) {
    if ([regex]::Matches($lockText,[regex]::Escape($lockMarker)).Count -ne 1) { throw 'Locktopus analog site missing or ambiguous' }
    $lockText=$lockText.Replace($lockMarker,$lockPatch)
}
if ($lockText -ne $lockOld.Replace("`r`n","`n")) { [IO.File]::WriteAllText($lockFile,$lockText) }

# Preserve manual/scripted close calls: gate ONLY update's timer call82552710.
# Exact LR and following label distinguish it from the shared helper's callers.
$doorFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.126.cpp'
$doorOld = [IO.File]::ReadAllText($doorFile).Replace("`r`n", "`n")
$doorText = $doorOld
$doorInclude = '#include "../../src/pdz_qol.h"'
if (-not $doorText.Contains($doorInclude)) { $doorText = "$doorInclude`n$doorText" }
$doorMarker = "`tctx.lr = 0x82552714;`n`tsub_82553640(ctx, base);`nloc_82552714:"
$doorPatch = "`tctx.lr = 0x82552714;`n`tif (PDZ_AllowTimedDoorClose(base, ctx.r31.u32)) {`n`t`tsub_82553640(ctx, base);`n`t}`nloc_82552714:"
if (-not $doorText.Contains($doorPatch)) {
    if (([regex]::Matches($doorText, [regex]::Escape($doorMarker))).Count -ne 1) {
        throw 'Door timed-close call82552710 not uniquely recognized; refusing broad close patch.'
    }
    $doorText = $doorText.Replace($doorMarker, $doorPatch)
}
if ($doorText -ne $doorOld) {
    [IO.File]::WriteAllText($doorFile, $doorText)
    Write-Host '[doors] reapplied warehouse-only timed-close gate'
}

# Observe exact created scope resources: never skip scene creation or updates.
$scopeFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.95.cpp'
$scopeOld = [IO.File]::ReadAllText($scopeFile).Replace("`r`n", "`n")
$scopeText = $scopeOld
if (-not $scopeText.Contains($doorInclude)) { $scopeText = "$doorInclude`n$scopeText" }
$scopeMarker = "`tctx.lr = 0x823A11F0;`n`tsub_82399048(ctx, base);"
$scopePatch = "$scopeMarker`n`tPDZ_ObserveScopeScene(base, ctx.r29.u32, ctx.r31.u32);"
if (-not $scopeText.Contains($scopePatch)) {
    if (([regex]::Matches($scopeText, [regex]::Escape($scopeMarker))).Count -ne 1) {
        throw 'Scope scene observer site not uniquely recognized.'
    }
    $scopeText = $scopeText.Replace($scopeMarker, $scopePatch)
}
if ($scopeText -ne $scopeOld) { [IO.File]::WriteAllText($scopeFile, $scopeText) }

$files = Get-ChildItem -LiteralPath $GeneratedDir -Filter "*.cpp"
$hudTraceFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.24.cpp'
$hudTraceText = [IO.File]::ReadAllText($hudTraceFile).Replace("`r`n", "`n")
$hudTraceText = $hudTraceText.Replace("`textern void PDZ_ObserveHudDraw(uint8_t*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, double, double);`n", '')
$hudTraceInclude = '#include "../../src/pdz_hud_bridge.h"'
if (-not $hudTraceText.Contains($hudTraceInclude)) {
    $hudTraceText = "$hudTraceInclude`n$hudTraceText"
}
$hudTraceEntry = "DEFINE_REX_FUNC(sub_8226AE90) {`n`tREX_FUNC_PROLOGUE();"
$hudTracePatch = "$hudTraceEntry`n`tPDZ_ObserveHudDraw(base, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r9.u32, uint32_t(ctx.lr), ctx.f1.f64, ctx.f2.f64);"
if (-not $hudTraceText.Contains($hudTracePatch)) {
    if (-not $hudTraceText.Contains($hudTraceEntry)) { throw 'HUD sprite observer site missing.' }
    $hudTraceText = $hudTraceText.Replace($hudTraceEntry, $hudTracePatch)
    Write-Host '[classic-hud] reapplied bounded native sprite observer'
}
if ($hudTraceText -ne [IO.File]::ReadAllText($hudTraceFile).Replace("`r`n", "`n")) {
    [IO.File]::WriteAllText($hudTraceFile, $hudTraceText)
}
$totalPatched = 0

# Scale native top-level HUD groups inside the native transform computation.
$hudScaleFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.35.cpp'
$hudScaleText = [IO.File]::ReadAllText($hudScaleFile).Replace("`r`n", "`n")
if (-not $hudScaleText.Contains($hudTraceInclude)) { $hudScaleText = "$hudTraceInclude`n$hudScaleText" }
$hudScaleEntry = "DEFINE_REX_FUNC(sub_8278BDB8) {`n`tREX_FUNC_PROLOGUE();"
$hudScalePatch = "$hudScaleEntry`n`tPDZ_NativeHudScaleScope pdz_hud_scale(base, ctx.r3.u32);"
if (-not $hudScaleText.Contains($hudScalePatch)) {
    if (-not $hudScaleText.Contains($hudScaleEntry)) { throw 'Native HUD transform site missing.' }
    $hudScaleText = $hudScaleText.Replace($hudScaleEntry,$hudScalePatch)
}
if ($hudScaleText -ne [IO.File]::ReadAllText($hudScaleFile).Replace("`r`n", "`n")) {
    [IO.File]::WriteAllText($hudScaleFile,$hudScaleText)
}

# Selectively replace only native XUI render messages for named HUD elements.
# No update/input/property/lifetime messages are bypassed.
$hudRenderFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.123.cpp'
$hudRenderText = [IO.File]::ReadAllText($hudRenderFile).Replace("`r`n", "`n")
if (-not $hudRenderText.Contains($hudTraceInclude)) {
    $hudRenderText = "$hudTraceInclude`n$hudRenderText"
}
$hudRenderEntry = "DEFINE_REX_FUNC(sub_82772B20) {`n`tREX_FUNC_PROLOGUE();"
$hudRenderPatch = "$hudRenderEntry`n`tif (PDZ_SuppressClassicHudRender(base, ctx.r3.u32, ctx.r4.u32)) {`n`t`tREX_STORE_U32(ctx.r4.u32 + 8, 1);`n`t`tctx.r3.u64 = 0;`n`t`treturn;`n`t}"
if (-not $hudRenderText.Contains($hudRenderPatch)) {
    if (-not $hudRenderText.Contains($hudRenderEntry)) { throw 'Native XUI HUD render hook site missing.' }
    $hudRenderText = $hudRenderText.Replace($hudRenderEntry, $hudRenderPatch)
    Write-Host '[classic-hud] reapplied named XUI render-message gate'
}
if ($hudRenderText -ne [IO.File]::ReadAllText($hudRenderFile).Replace("`r`n", "`n")) {
    [IO.File]::WriteAllText($hudRenderFile, $hudRenderText)
}
# XuiElementRenderChildren uses the fast/handled-message dispatcher directly,
# not sub_82772B20. Gate its render message identically, before lifetime changes.
$hudFastFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.20.cpp'
$hudFastText = [IO.File]::ReadAllText($hudFastFile).Replace("`r`n", "`n")
if (-not $hudFastText.Contains($hudTraceInclude)) {
    $hudFastText = "$hudTraceInclude`n$hudFastText"
}
$hudFastEntry = "DEFINE_REX_FUNC(sub_827729F8) {`n`tREX_FUNC_PROLOGUE();"
$hudFastPatch = $hudRenderPatch.Replace($hudRenderEntry,$hudFastEntry)
if (-not $hudFastText.Contains($hudFastPatch)) {
    if (-not $hudFastText.Contains($hudFastEntry)) { throw 'Native XUI child-render hook site missing.' }
    $hudFastText = $hudFastText.Replace($hudFastEntry,$hudFastPatch)
    Write-Host '[classic-hud] reapplied named XUI child-render gate'
}
if ($hudFastText -ne [IO.File]::ReadAllText($hudFastFile).Replace("`r`n", "`n")) {
    [IO.File]::WriteAllText($hudFastFile,$hudFastText)
}

foreach ($file in $files) {
    $lines = [System.IO.File]::ReadAllLines($file.FullName)

    $declared = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($line in $lines) {
        if ($line -match '^(loc_[0-9A-Fa-f]+):') {
            [void]$declared.Add($Matches[1])
        }
    }

    $changed = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ($line -match 'goto (loc_[0-9A-Fa-f]+);' -and -not $declared.Contains($Matches[1])) {
            $label = $Matches[1]
            $lines[$i] = $line -replace "goto $label;", `
                "{ REXLOG_WARN(""unresolved tail-jump to $label (shared tail block in a different generated function)""); return; }"
            $changed = $true
            $totalPatched++
            Write-Host "  [fix-tail-jumps] $($file.Name): patched unresolved goto $label"
        } elseif ($line -match '^(\s*)if \((.+)\) REX_FATAL\("Unresolved branch from 0x[0-9A-Fa-f]+ to 0x([0-9A-Fa-f]+)"\);$') {
            $indent = $Matches[1]
            $condition = $Matches[2]
            $label = "loc_$($Matches[3].ToUpperInvariant())"
            if ($declared.Contains($label)) {
                $lines[$i] = "${indent}if ($condition) goto $label;"
                $changed = $true
                $totalPatched++
                Write-Host "  [fix-tail-jumps] $($file.Name): restored false unresolved branch to $label"
            }
        }
    }

    if ($changed) {
        [System.IO.File]::WriteAllLines($file.FullName, $lines)
    }
}
Write-Host "[fix-tail-jumps] done, $totalPatched unresolved tail-jump(s) patched"

# Remove the retired identity mode hook from historical generated output.
& (Join-Path $PSScriptRoot 'patch-vision-color.ps1') -GeneratedDir $GeneratedDir

# RCP90 mode-2 composite-state trace. Codegen preserves this file when its
# inputs are unchanged, but this block also reapplies the observational hook
# after a future regeneration.
$visionFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.99.cpp'
if (Test-Path $visionFile) {
    $text = [IO.File]::ReadAllText($visionFile)
    $marker = "`tctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 732);"
    $trace = "$marker`n`tPDZ_TraceVisionState(base, ctx.r31.u32);"
    if (-not $text.Contains('PDZ_TraceVisionState(base, ctx.r31.u32);')) {
        if (-not $text.Contains($marker)) { throw 'RCP90 vision-mode hook site was not recognized.' }
        [IO.File]::WriteAllText($visionFile, $text.Replace($marker, $trace))
        Write-Host '[rcp90] added mode-2 composite-state trace'
    }
}

$submissionFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.123.cpp'
if (Test-Path $submissionFile) {
    $text = [IO.File]::ReadAllText($submissionFile)
    $entry = "DEFINE_REX_FUNC(sub_8260F6B0) {`n`tREX_FUNC_PROLOGUE();"
    $trace = "$entry`n`tPDZ_TraceRcpSubmission(base, ctx.r3.u32, ctx.r4.u32);"
    if (-not $text.Contains('PDZ_TraceRcpSubmission(base, ctx.r3.u32, ctx.r4.u32);')) {
        if (-not $text.Contains($entry)) { throw 'RCP90 submission hook site was not recognized.' }
        [IO.File]::WriteAllText($submissionFile, $text.Replace($entry, $trace))
        Write-Host '[rcp90] added submission trace'
    }
}

# Keep the Xbox target layout within its original memory limits while the GPU
# rasterizes at 2x. 960x540 guest scene targets therefore render at 1920x1080.
# Patch the shared scene dimensions before any dependent targets are allocated;
# leave the SD 640x480 target pair and the presentation/HUD dimensions intact.
$resolutionFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.102.cpp'
$text = [IO.File]::ReadAllText($resolutionFile)
$marker = "`t// bl 0x8260e460`n`tctx.lr = 0x8260E9D8;"
$resolutionHook = @'
	// PDZ exact 1080p scene target layout.
	if (rex::cvar::Query<bool>("pdz_internal_1080p")) {
		const auto shared = rex::cvar::Query<int32_t>("resolution_scale");
		const auto sx = rex::cvar::HasNonDefaultValue("draw_resolution_scale_x")
			? rex::cvar::Query<int32_t>("draw_resolution_scale_x") : shared;
		const auto sy = rex::cvar::HasNonDefaultValue("draw_resolution_scale_y")
			? rex::cvar::Query<int32_t>("draw_resolution_scale_y") : shared;
		if (sx == 2 && sy == 2) {
			REX_STORE_U32(ctx.r31.u32 + 112, 960);
			REX_STORE_U32(ctx.r31.u32 + 108, 540);
			REX_STORE_U32(ctx.r25.u32 - 19780, 960);
			REX_STORE_U32(ctx.r28.u32 - 19784, 540);
			REXLOG_INFO("PDZ scene layout: 960x540, requested GPU scale 2x2, scene raster 1920x1080");
		} else {
			REXLOG_WARN("PDZ 1080p requires GPU scale 2x2; retaining native scene layout");
		}
		rex::FlushLogging();
	}
'@
$resolutionHook = $resolutionHook.Replace("`r`n", "`n")
if (-not $text.Contains('// PDZ exact 1080p scene target layout.')) {
    if (-not $text.Contains($marker)) { throw 'Scene target initialization was not recognized.' }
    [IO.File]::WriteAllText($resolutionFile, $text.Replace($marker, "$resolutionHook`n$marker"))
    Write-Host '[resolution] added exact 1080p scene target layout'
}

# DarkCrosshair's draw method chooses the reticle texture and emits its screen
# geometry. Keep its separate update/creation methods active so toggling back
# restores the current reticle without rebuilding its state.
$crosshairFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.67.cpp'
$text = [IO.File]::ReadAllText($crosshairFile)
$entry = "DEFINE_REX_FUNC(sub_8225CB18) {`n`tREX_FUNC_PROLOGUE();"
$patchedEntry = "$entry`n`tif (rex::cvar::Query<bool>(`"pdz_hide_original_crosshair`")) return;"
if (-not $text.Contains($patchedEntry) -and -not $text.Contains("PDZ_ObserveOriginalCrosshair(base, ctx.r3.u32,")) {
    if (-not $text.Contains($entry)) { throw 'Crosshair draw entry was not recognized.' }
    [IO.File]::WriteAllText($crosshairFile, $text.Replace($entry, $patchedEntry))
    Write-Host '[crosshair] added live original-crosshair visibility toggle'
}

# The derived reticle renderer calls the base texture renderer and then draws
# the additional spread arms. Gate it too or those arms remain visible.
$crosshairFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.13.cpp'
$text = [IO.File]::ReadAllText($crosshairFile)
$entry = "DEFINE_REX_FUNC(sub_8225D280) {`n`tREX_FUNC_PROLOGUE();"
$patchedEntry = "$entry`n`tif (rex::cvar::Query<bool>(`"pdz_hide_original_crosshair`")) return;"
if (-not $text.Contains($patchedEntry) -and -not $text.Contains("PDZ_ObserveOriginalCrosshair(base, ctx.r3.u32,")) {
    if (-not $text.Contains($entry)) { throw 'Derived crosshair draw entry was not recognized.' }
    [IO.File]::WriteAllText($crosshairFile, $text.Replace($entry, $patchedEntry))
    Write-Host '[crosshair] added visibility toggle for spread reticles'
}

# Observe the native reticle decision even when original drawing is hidden.
foreach ($item in @(@{Part='67'; Name='sub_8225CB18'; Spread='false'}, @{Part='13'; Name='sub_8225D280'; Spread='true'})) {
    $file = Join-Path $GeneratedDir "perfectdarkzerorecomp_recomp.$($item.Part).cpp"
    $source = [IO.File]::ReadAllText($file)
    $originalSource = $source
    $oldHide = 'if (rex::cvar::Query<bool>("pdz_hide_original_crosshair")) return;'
    $classicHide = 'if (rex::cvar::Query<bool>("pdz_hide_original_crosshair") || (rex::cvar::Query<bool>("pdz_classic_hud") && rex::cvar::Query<bool>("pdz_classic_hud_crosshair"))) return;'
    $source = $source.Replace($oldHide,$classicHide)
    $entry = "DEFINE_REX_FUNC($($item.Name)) {`n`tREX_FUNC_PROLOGUE();"
    $observed = "$entry`n`textern void PDZ_ObserveOriginalCrosshair(uint8_t*, uint32_t, bool);`n`tPDZ_ObserveOriginalCrosshair(base, ctx.r3.u32, $($item.Spread));"
    if (-not $source.Contains($observed)) {
        if (-not $source.Contains($entry)) { throw 'Native crosshair observer entry missing.' }
        $source = $source.Replace($entry, $observed)
        Write-Host '[crosshair] observing native armed-reticle visibility'
    }
    if ($source -ne $originalSource) { [IO.File]::WriteAllText($file,$source) }
}
# Some guest worker paths can enter translated vector floating-point code with
# host exceptions unmasked. Keep the guest rounding/flush bits while ensuring
# every translated function establishes the runtime's host FP policy.
$pch = Join-Path $GeneratedDir 'perfectdarkzerorecomp_pch.h'
if (Test-Path $pch) {
    $text = [IO.File]::ReadAllText($pch)
    $nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $old = '#define REX_FUNC_PROLOGUE() __builtin_assume(((size_t)base & 0x1F) == 0)'
    $new = '#define REX_FUNC_PROLOGUE() do { ctx.fpscr.InitHost(); __builtin_assume(((size_t)base & 0x1F) == 0); } while (0)'
    $profileOld = "#define REX_FUNC_PROLOGUE()                     \$nl  __builtin_assume(((size_t)base & 0x1F) == 0); \$nl  ZoneNamedN(___tracy_guest_zone, __func__, TracyIsStarted)"
    $profileNew = "#define REX_FUNC_PROLOGUE()                     \$nl  ctx.fpscr.InitHost();                         \$nl  __builtin_assume(((size_t)base & 0x1F) == 0); \$nl  ZoneNamedN(___tracy_guest_zone, __func__, TracyIsStarted)"
    if ($text.Contains($old)) {
        $text = $text.Replace($old, $new)
    } elseif (-not $text.Contains($new)) {
        throw 'Generated PCH function-prologue layout was not recognized.'
    }
    if ($text.Contains($profileOld)) {
        $text = $text.Replace($profileOld, $profileNew)
    } elseif (-not $text.Contains($profileNew)) {
        throw 'Generated PCH profiling function-prologue layout was not recognized.'
    }
    if ([IO.File]::ReadAllText($pch) -cne $text) {
        [IO.File]::WriteAllText($pch, $text)
    }
    Write-Host '[fix-runtime] masked host FP exceptions in generated function prologues'

    if (-not $text.Contains('#include "src/pdz_qol.h"')) {
        $marker = '#include <rex/ppc.h>'
        if (-not $text.Contains($marker)) { throw 'Generated PCH include layout was not recognized.' }
        [IO.File]::WriteAllText($pch, $text.Replace($marker, "$marker`n#include `"src/pdz_qol.h`""))
        Write-Host '[fix-mouse] exposed PDZ quality-of-life hooks to generated code'
    }
}

# Reapply the Xenia PDZ 60 FPS patch at guest address 0x826CFB77. The address
# is the low byte of `li r10,2` at 0x826CFB74; translating it means selecting
# a vblank divisor of 1 instead of 2. Keep it runtime-configurable.
$fpsFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.140.cpp'
if (Test-Path $fpsFile) {
    $text = [IO.File]::ReadAllText($fpsFile)
    $pattern = 'loc_826CFB74:\r?\n\t// li r10,2\r?\n\tctx\.r10\.s64 = 2;'
    $patched = "loc_826CFB74:`n`t// li r10,2; Xenia patch 0x826CFB77 changes the immediate to 1.`n`tctx.r10.s64 = rex::cvar::Query<bool>(`"pdz_fps60_unlock`") ? 1 : 2;"
    if ([regex]::IsMatch($text, $pattern)) {
        [IO.File]::WriteAllText($fpsFile, [regex]::Replace($text, $pattern, $patched, 1))
        Write-Host '[fix-fps] applied runtime-configurable PDZ vblank-divisor patch'
    } elseif (-not $text.Contains($patched)) {
        throw 'Generated 60 FPS patch site was not recognized.'
    }
}


# Bypass PDZ's right-stick conditioner for relative mouse motion. The output
# pointers are preserved in r25/r24 through sub_8226BE48.
$mouseFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.167.cpp'
if (Test-Path $mouseFile) {
    $text = [IO.File]::ReadAllText($mouseFile)
    $entry = "DEFINE_REX_FUNC(sub_8226BE48) {`n`tREX_FUNC_PROLOGUE();"
    $trace = "$entry`n`tPDZ_TraceStickCall(ctx.r3.u32, uint32_t(ctx.lr), ctx.r4.u32, ctx.r5.u32);"
    $oldTrace = "$entry`n`tPDZ_TraceStickCall(ctx.r3.u32, ctx.lr.u32, ctx.r4.u32, ctx.r5.u32);"
    if ($text.Contains($oldTrace)) {
        $text = $text.Replace($oldTrace, $trace)
        [IO.File]::WriteAllText($mouseFile, $text)
        Write-Host '[trace-mouse] corrected PDZ link-register trace access'
    }
    if ($text.Contains($entry) -and -not $text.Contains($trace)) {
        $text = $text.Replace($entry, $trace)
        [IO.File]::WriteAllText($mouseFile, $text)
        Write-Host '[trace-mouse] recording unique PDZ stick-conditioner callers'
    } elseif (-not $text.Contains($trace)) {
        throw 'Generated stick trace entry site was not recognized.'
    }
    $marker = "loc_8226C22C:`r`n`t// addi r1,r1,304"
    if (-not $text.Contains($marker)) { $marker = "loc_8226C22C:`n`t// addi r1,r1,304" }
    $call = "loc_8226C22C:`n`tPDZ_ApplyDirectMouse(base, ctx.r25.u32, ctx.r24.u32, ctx.r29.u32 / 60);`n`t// addi r1,r1,304"
    $oldCall = "loc_8226C22C:`n`tPDZ_ApplyDirectMouse(base, ctx.r25.u32, ctx.r24.u32);`n`t// addi r1,r1,304"
    if ($text.Contains($marker)) {
        [IO.File]::WriteAllText($mouseFile, $text.Replace($marker, $call))
        Write-Host '[fix-mouse] bypassed PDZ analog-stick conditioner for relative mouse motion'
    } elseif ($text.Contains($oldCall)) {
        [IO.File]::WriteAllText($mouseFile, $text.Replace($oldCall, $call))
        Write-Host '[fix-mouse] restricted PDZ mouse bypass to the right-stick pass'
    } elseif (-not $text.Contains('PDZ_ApplyDirectMouse(base, ctx.r25.u32, ctx.r24.u32, ctx.r29.u32 / 60);')) {
        throw 'Generated direct-mouse patch site was not recognized.'
    }
}

# Observe the cover update immediately before it consumes player pitch/yaw and
# again after all camera branches complete. This is diagnostic only: the failed
# retained-angle override is deliberately removed.
$coverCameraFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.23.cpp'
if (Test-Path $coverCameraFile) {
    $text = [IO.File]::ReadAllText($coverCameraFile)
    $entry = "DEFINE_REX_FUNC(sub_824F2E20) {`n`tREX_FUNC_PROLOGUE();"
    $oldPatchedEntry = "$entry`n`tPDZ_ApplyCoverAimOverride(base, ctx.r3.u32);"
    $oldConsumePoint = "loc_824F3340:`n`tPDZ_ApplyCoverAimOverride(base, ctx.r31.u32);`n`t// clrlwi. r11,r28,24"
    $consumePoint = "loc_824F3340:`n`t// clrlwi. r11,r28,24"
    $patchedConsumePoint = "loc_824F3340:`n`tPDZ_TraceCoverCamera(base, ctx.r31.u32, ctx.r25.u32, ctx.r28.u32, 0);`n`t// clrlwi. r11,r28,24"
    $exitPoint = "loc_824F4578:`n`t// lfs f0,7328(r31)"
    $patchedExitPoint = "loc_824F4578:`n`tPDZ_TraceCoverCamera(base, ctx.r31.u32, ctx.r25.u32, ctx.r28.u32, 1);`n`t// lfs f0,7328(r31)"
    if ($text.Contains($oldPatchedEntry)) {
        $text = $text.Replace($oldPatchedEntry, $entry)
    }
    if ($text.Contains($oldConsumePoint)) {
        $text = $text.Replace($oldConsumePoint, $consumePoint)
    }
    if ($text.Contains($consumePoint) -and -not $text.Contains($patchedConsumePoint)) {
        $text = $text.Replace($consumePoint, $patchedConsumePoint)
    } elseif (-not $text.Contains($patchedConsumePoint)) {
        throw 'Cover-camera post-overwrite consumption point was not recognized.'
    }
    if ($text.Contains($exitPoint) -and -not $text.Contains($patchedExitPoint)) {
        $text = $text.Replace($exitPoint, $patchedExitPoint)
    } elseif (-not $text.Contains($patchedExitPoint)) {
        throw 'Cover-camera final output point was not recognized.'
    }
    if ([IO.File]::ReadAllText($coverCameraFile) -cne $text) {
        [IO.File]::WriteAllText($coverCameraFile, $text)
    }
    Write-Host '[trace-cover-camera] paired input and final cover-camera state'
}

# Remove the rejected submission-branch hooks. The alternate submission path
# bypasses both; corrections belong before the shared view/inverse upload.
$viewMatrixFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.155.cpp'
if (Test-Path $viewMatrixFile) {
    $text = [IO.File]::ReadAllText($viewMatrixFile)
    foreach ($returnAddress in @('0x8220686C', '0x822068B8')) {
        $marker = "`tctx.lr = $returnAddress;`n`tsub_825F32A8(ctx, base);"
        $legacyPatched = "`tctx.lr = $returnAddress;`n`tPDZ_AdjustFinalCoverView(base, ctx.r30.u32 + 1024);`n`tsub_825F32A8(ctx, base);"
        $patched = "`tctx.lr = $returnAddress;`n`tPDZ_AdjustFinalCoverView(base, ctx.r30.u32 + 1024, $returnAddress);`n`tsub_825F32A8(ctx, base);"
        if ($text.Contains($marker)) {
            # Already clean.
        } elseif ($text.Contains($legacyPatched)) {
            $text = $text.Replace($legacyPatched, $marker)
        } elseif ($text.Contains($patched)) {
            $text = $text.Replace($patched, $marker)
        } else {
            throw "Final cover-view consumer $returnAddress was not recognized."
        }
    }
    if ([IO.File]::ReadAllText($viewMatrixFile) -cne $text) {
        [IO.File]::WriteAllText($viewMatrixFile, $text)
    }
    Write-Host '[cover-camera] removed rejected submission-branch hooks'
}

$viewProducerFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.12.cpp'
if (Test-Path $viewProducerFile) {
    $text = [IO.File]::ReadAllText($viewProducerFile)
    $marker = "`tctx.lr = 0x82205968;`n`tsub_82643740(ctx, base);"
    $patched = "`tctx.lr = 0x82205968;`n`tPDZ_AdjustFinalCoverView(base, ctx.r28.u32, 0x82205968);`n`tsub_82643740(ctx, base);"
    if ($text.Contains($marker)) {
        $text = $text.Replace($marker, $patched)
    } elseif (-not $text.Contains($patched)) {
        throw 'Shared cover-view upload point was not recognized.'
    }
    if ([IO.File]::ReadAllText($viewProducerFile) -cne $text) {
        [IO.File]::WriteAllText($viewProducerFile, $text)
    }
    Write-Host '[cover-camera] correct newly composed view before shared upload'
}

# Match the PDZ-specific Xenia MouseHook patch which decouples first-person
# weapon sway from the right-stick camera values. TU0's two tiny accessors
# originally read +0xF54/+0xF58; the hook redirects them to the dedicated
# mouse camera values at +0xF9C/+0xFA0.
$gunAccessorSites = @(
    @('perfectdarkzerorecomp_recomp.101.cpp', 'sub_8253EDA0', 3924, 3996),
    @('perfectdarkzerorecomp_recomp.105.cpp', 'sub_8253EDA8', 3928, 4000)
)
# Add turn lag only to the local-player gun rotation, before its Euler matrix
# builder. One exact call site; never patch the shared camera/math builder.
$swayFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.135.cpp'
$swayText = [IO.File]::ReadAllText($swayFile).Replace("`r`n", "`n")
$swayInclude = '#include "../../src/pdz_qol.h"'
if (-not $swayText.Contains($swayInclude)) {
    $swayText = $swayText.Replace('#include "perfectdarkzerorecomp_funcs.135.h"', "#include `"perfectdarkzerorecomp_funcs.135.h`"`n$swayInclude")
}
$swayMarker = "`tctx.lr = 0x825A3DA0;`n`tsub_826F4828(ctx, base);"
$swayPatch = "`tctx.lr = 0x825A3DA0;`n`tPDZ_ApplyClassicWeaponSway(base, ctx.r31.u32, ctx.f1.f64, ctx.f2.f64);`n`tsub_826F4828(ctx, base);"
if ($swayText.Contains($swayMarker)) { $swayText = $swayText.Replace($swayMarker,$swayPatch) }
if (-not $swayText.Contains($swayPatch)) { throw 'Classic weapon rotation call site was not recognized.' }
[IO.File]::WriteAllText($swayFile,$swayText)

# Separate procedural translation amplitude from cursor-linked rotation.
$bobMarker = "loc_825A40AC:"
$bobPatch = "$bobMarker`n`tPDZ_AdjustWeaponBob(base, ctx.r31.u32, ctx.r30.u32, ctx.f25.f64, ctx.f26.f64);"
if (-not $swayText.Contains($bobPatch)) {
    if (-not $swayText.Contains($bobMarker)) { throw 'Weapon bob site not recognized.' }
    $swayText = $swayText.Replace($bobMarker,$bobPatch)
}
# The stack matrix is freshly constructed, then copied into this hand's render
# transform. Keep all skeleton/animation work and native update/bounds handling.
$handsMarker = "`tctx.lr = 0x825A4248;`n`tsub_82391510(ctx, base);"
$handsPatch = "`tctx.lr = 0x825A4248;`n`tPDZ_AdjustUnarmedViewmodel(base, ctx.r31.u32, ctx.r28.u32, ctx.r1.u32 + 160);`n`tsub_82391510(ctx, base);"
if (-not $swayText.Contains($handsPatch)) {
    if (-not $swayText.Contains($handsMarker)) { throw 'Unarmed viewmodel site not recognized.' }
    $swayText = $swayText.Replace($handsMarker,$handsPatch)
}
[IO.File]::WriteAllText($swayFile,$swayText)
# Empty hands use cached weapon meshes, NOT the attached model5272. Native
# r21/r22 feed their masks; preserve body/head r17/r18/r19/r20 unchanged.
$fistsFile=Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.163.cpp'
$fistsText=[IO.File]::ReadAllText($fistsFile).Replace("`r`n","`n")
if(-not $fistsText.Contains($swayInclude)) {
    $fistsText=$fistsText.Replace('#include "perfectdarkzerorecomp_funcs.163.h"',"#include `"perfectdarkzerorecomp_funcs.163.h`"`n$swayInclude")
}
$fistsText=$fistsText.Replace("`n`tctx.r17.u64 = PDZ_IdleFistVisibility(base, ctx.r31.u32, ctx.r17.u32);",'')
$fistsMarker="loc_8259917C:"
$fistsPatch="$fistsMarker`n`tctx.r21.u64 = PDZ_IdleFistVisibility(base, ctx.r31.u32, ctx.r21.u32);`n`tctx.r22.u64 = PDZ_IdleFistVisibility(base, ctx.r31.u32, ctx.r22.u32);"
if(-not $fistsText.Contains($fistsPatch)) {
    if(-not $fistsText.Contains($fistsMarker)){throw 'Dedicated first-person fist visibility site not recognized.'}
    $fistsText=$fistsText.Replace($fistsMarker,$fistsPatch)
}
[IO.File]::WriteAllText($fistsFile,$fistsText)
# Coupled screen-space offset at the proven local-owner aim producer, after
# native spread and before native unprojection/collision; no NPC-wide ray hook.
$aimFile=Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.112.cpp'
$aimText=[IO.File]::ReadAllText($aimFile).Replace("`r`n","`n")
if(-not $aimText.Contains($swayInclude)) {
    $aimText=$aimText.Replace('#include "perfectdarkzerorecomp_funcs.112.h"',"#include `"perfectdarkzerorecomp_funcs.112.h`"`n$swayInclude")
}
$aimMarker="`tctx.lr = 0x822399C0;`n`tsub_82644660(ctx, base);"
$spreadMarker="`t// fmadds f29,f0,f31,f25"
$spreadPatch="`tPDZ_AdjustWeaponSpread(base, ctx.r31.u32, ctx.r29.u32, ctx.f31.f64);`n$spreadMarker"
if(-not $aimText.Contains($spreadPatch)) {
    if(-not $aimText.Contains($spreadMarker)){throw 'Native random-spread application not recognized.'}
    $aimText=$aimText.Replace($spreadMarker,$spreadPatch)
}
$aimPatch="`tctx.lr = 0x822399C0;`n`tPDZ_ApplySwayAim(base, ctx.r31.u32, ctx.r3.u32, ctx.r4.u32, ctx.f25.f64, ctx.f24.f64, ctx.f1.f64);`n`tsub_82644660(ctx, base);"
if($aimText.Contains($aimMarker)) { $aimText=$aimText.Replace($aimMarker,$aimPatch) }
if(-not $aimText.Contains($aimPatch)) { throw 'Coupled sway aim unprojection call was not recognized.' }
$orderMarker="`tctx.lr = 0x822399C0;`n`tPDZ_ApplySwayAim"
$orderPrefix="`tPDZ_TraceCoverAimOrder(base, ctx.r31.u32, 0, ctx.r4.u32, 0, 2, ctx.r3.u32);`n`tPDZ_TraceCoverAimOrder(base, ctx.r31.u32, 4, ctx.r3.u32 + 112, ctx.r3.u32 + 176, 16, ctx.r3.u32);`n$orderMarker"
if(-not $aimText.Contains($orderPrefix)) {
    if([regex]::Matches($aimText,[regex]::Escape($orderMarker)).Count -ne 1){throw 'Cover order input marker must be unique.'}
    $aimText=$aimText.Replace($orderMarker,$orderPrefix)
}
# At CF0 the native collision-hit branch has copied stack+260 to r27.
# r31 has been repurposed; stage0's thread-local identity supplies the owner.
$orderFinal="loc_82239CF0:"
$orderFinalPatch="$orderFinal`n`tPDZ_TraceCoverAimOrder(base, 0, 3, ctx.r27.u32, ctx.r1.u32 + 96, 3, 0);"
$resolvedMarker="`tPDZ_TraceCoverAimOrder(base, 0, 3, ctx.r27.u32, ctx.r1.u32 + 96, 3, 0);"
# Clearance legitimately sits between the label and observer. Adjacency is not
# an idempotence test: it used to reinsert both hooks on every adapter pass.
if([regex]::Matches($aimText,[regex]::Escape($resolvedMarker)).Count -gt 1){throw 'Duplicated cover final observer; use fresh generation or reviewed restoration.'}
if(-not $aimText.Contains($resolvedMarker)) {
    if([regex]::Matches($aimText,[regex]::Escape($orderFinal)).Count -ne 1){throw 'Cover order final target marker must be unique.'}
    $aimText=$aimText.Replace($orderFinal,$orderFinalPatch)
}
$resolvedPatch="$resolvedMarker`n`tPDZ_RecordCoverTarget(base, ctx.r27.u32);"
if(-not $aimText.Contains($resolvedPatch)) {
    if([regex]::Matches($aimText,[regex]::Escape($resolvedMarker)).Count -ne 1){throw 'Cover resolved target marker must be unique.'}
    $aimText=$aimText.Replace($resolvedMarker,$resolvedPatch)
}
$orderHit="`tctx.lr = 0x82239CD0;`n`tREX_CALL_INDIRECT_FUNC(ctx.ctr.u32);"
$orderHitPatch="$orderHit`n`tPDZ_TraceCoverAimOrder(base, 0, 6, ctx.r27.u32, 0, 3, ctx.r3.u32 & 0xFF);"
if(-not $aimText.Contains($orderHitPatch)) {
    if([regex]::Matches($aimText,[regex]::Escape($orderHit)).Count -ne 1){throw 'Cover order hit marker must be unique.'}
    $aimText=$aimText.Replace($orderHit,$orderHitPatch)
}
$orderReturn="`tctx.lr = 0x82239DC4;"
# Immediately before stack release: native returned endpoint has been written.
$orderReturnMarker="`t// addi r1,r1,432"
$functionStart=$aimText.IndexOf('DEFINE_REX_FUNC(sub_822397C8)')
$functionEnd=$aimText.IndexOf($orderReturn,$functionStart)
if($functionStart -lt 0 -or $functionEnd -lt 0){throw 'Cover order return function missing.'}
$functionPart=$aimText.Substring($functionStart,$functionEnd-$functionStart)
$orderReturnPatch="`tPDZ_TraceCoverAimOrder(base, 0, 5, ctx.r27.u32, 0, 3, 0);`n$orderReturnMarker"
if(-not $functionPart.Contains($orderReturnPatch)) {
    if([regex]::Matches($functionPart,[regex]::Escape($orderReturnMarker)).Count -ne 1){throw 'Cover order return marker must be unique.'}
    $aimText=$aimText.Remove($functionStart,$functionPart.Length).Insert($functionStart,$functionPart.Replace($orderReturnMarker,$orderReturnPatch))
}
$coverRayMarker="`t// lwz r11,8(r31)`n`tctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);`n`t// lwz r11,0(r11)`n`tctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);`n`t// rlwinm r11,r11,30,31,31"
$coverRayHook="`tPDZ_AdjustCoverTargetRay(base, ctx.r31.u32, ctx.r1.u32 + 80, ctx.r1.u32 + 96, ctx.r27.u32, ctx.f27.f64, ctx.f28.f64, ctx.f29.f64);`n$coverRayMarker"
if(-not $aimText.Contains($coverRayHook)) {
    if([regex]::Matches($aimText,[regex]::Escape($coverRayMarker)).Count -ne 1){throw 'Cover target-ray insertion must be unique.'}
    $aimText=$aimText.Replace($coverRayMarker,$coverRayHook)
}
$ownerMarker='DEFINE_REX_FUNC(sub_822397C8) {'
$ownerPatch="$ownerMarker`n`tconst uint32_t pdz_target_player = ctx.r3.u32;"
if(-not $aimText.Contains($ownerPatch)) {
    if([regex]::Matches($aimText,[regex]::Escape($ownerMarker)).Count -ne 1){throw 'Target owner marker must be unique.'}
    $aimText=$aimText.Replace($ownerMarker,$ownerPatch)
}
$clearanceMarker='loc_82239CF0:'
$clearancePatch="$clearanceMarker`n`tPDZ_ConstrainThirdPersonTarget(ctx, base, pdz_target_player, ctx.r27.u32);"
if(-not $aimText.Contains($clearancePatch)) {
    if([regex]::Matches($aimText,[regex]::Escape($clearanceMarker)).Count -ne 1){throw 'Clearance return marker must be unique.'}
    $aimText=$aimText.Replace($clearanceMarker,$clearancePatch)
}
# Remove the retired diagnostic from previously adapted output too.
$aimText=$aimText.Replace("`tPDZ_TraceThirdPersonSceneRay(base, ctx.r31.u32, ctx.r4.u32, ctx.r5.u32);`n", '')
[IO.File]::WriteAllText($aimFile,$aimText)
$assistFile=Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.59.cpp'
$assistText=[IO.File]::ReadAllText($assistFile).Replace("`r`n","`n")
if(-not $assistText.Contains($swayInclude)) {
    $assistText=$assistText.Replace('#include "perfectdarkzerorecomp_funcs.59.h"',"#include `"perfectdarkzerorecomp_funcs.59.h`"`n$swayInclude")
}
$assistMarker="loc_825A366C:`n`t// lis r11,-32249"
$assistPatch="loc_825A366C:`n`tPDZ_ApplyAutoAimSetting(base, ctx.r31.u32);`n`t// lis r11,-32249"
if($assistText.Contains($assistMarker)) { $assistText=$assistText.Replace($assistMarker,$assistPatch) }
if(-not $assistText.Contains($assistPatch)) { throw 'Native aim-point application site was not recognized.' }
[IO.File]::WriteAllText($assistFile,$assistText)
$assistGateFile=Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.133.cpp'
$assistGateText=[IO.File]::ReadAllText($assistGateFile).Replace("`r`n","`n")
if(-not $assistGateText.Contains($swayInclude)) {
    $assistGateText=$assistGateText.Replace('#include "perfectdarkzerorecomp_funcs.133.h"',"#include `"perfectdarkzerorecomp_funcs.133.h`"`n$swayInclude")
}
$assistGateMarker="`tctx.lr = 0x824E2320;`n`tREX_CALL_INDIRECT_FUNC(ctx.ctr.u32);"
$assistGateOld="$assistGateMarker`n`tctx.r3.u64 = PDZ_AutoAimApplicationMode(base, REX_LOAD_U32(ctx.r31.u32 + 12), ctx.r3.u32);"
$assistGatePrevious="$assistGateMarker`n`tctx.r3.u64 = PDZ_AutoAimApplicationMode(base, REX_LOAD_U32(ctx.r31.u32 + 12), ctx.r3.u32, ctx.f31.f64);"
$assistGatePatch="$assistGateMarker`n`tctx.r3.u64 = PDZ_AutoAimApplicationMode(base, REX_LOAD_U32(ctx.r31.u32 + 12), ctx.r3.u32, ctx.f31.f64, ctx.r31.u32);"
if($assistGateText.Contains($assistGatePrevious)) { $assistGateText=$assistGateText.Replace($assistGatePrevious,$assistGatePatch) }
if($assistGateText.Contains($assistGateOld)) { $assistGateText=$assistGateText.Replace($assistGateOld,$assistGatePatch) }
if(-not $assistGateText.Contains($assistGatePatch) -and $assistGateText.Contains($assistGateMarker)) {
    $assistGateText=$assistGateText.Replace($assistGateMarker,$assistGatePatch)
}
if(-not $assistGateText.Contains($assistGatePatch)) { throw 'Native aim attraction gate site was not recognized.' }
$assistExitMarker="loc_824E250C:`n`t// li r3,0"
$assistExitPatch="loc_824E250C:`n`tPDZ_ObserveAutoAim(base, ctx.r31.u32, (ctx.r22.u32 & 255) != 0);`n`t// li r3,0"
if($assistGateText.Contains($assistExitMarker)) { $assistGateText=$assistGateText.Replace($assistExitMarker,$assistExitPatch) }
if(-not $assistGateText.Contains($assistExitPatch)) { throw 'Native aim completion site was not recognized.' }
[IO.File]::WriteAllText($assistGateFile,$assistGateText)
$retentionFile=Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.58.cpp'
$retentionText=[IO.File]::ReadAllText($retentionFile).Replace("`r`n","`n")
if(-not $retentionText.Contains($swayInclude)) {
  $retentionText=$retentionText.Replace('#include "perfectdarkzerorecomp_funcs.58.h"',"#include `"perfectdarkzerorecomp_funcs.58.h`"`n$swayInclude")
}
$retentionMarker="`tctx.lr = 0x824E2A18;`n`tsub_82644608(ctx, base);"
$retentionPatch="`tctx.lr = 0x824E2A18;`n`tPDZ_AdjustAimRetentionBounds(base, ctx.r28.u32, ctx.r31.u32, ctx.r3.u32, ctx.f31.f64, ctx.f30.f64, ctx.f0.f64, ctx.f13.f64);`n`tsub_82644608(ctx, base);"
if($retentionText.Contains($retentionMarker)) { $retentionText=$retentionText.Replace($retentionMarker,$retentionPatch) }
if(-not $retentionText.Contains($retentionPatch)) { throw 'Native per-candidate retention rectangle site was not recognized.' }
[IO.File]::WriteAllText($retentionFile,$retentionText)
foreach ($site in $gunAccessorSites) {
    $path = Join-Path $GeneratedDir $site[0]
    if (-not (Test-Path $path)) { continue }
    $text = [IO.File]::ReadAllText($path)
    $oldComment = "// lfs f1,$($site[2])(r3)"
    $newComment = "// lfs f1,$($site[3])(r3); PDZ MouseHook gun-sway decoupling"
    $oldLoad = "REX_LOAD_U32(ctx.r3.u32 + $($site[2]))"
    $newLoad = "REX_LOAD_U32(ctx.r3.u32 + $($site[3]))"
    $axis = if ($site[1] -eq 'sub_8253EDA0') { 1 } else { 0 }
    $valueLine = "`tctx.f1.f64 = double(temp.f32);"
    $adjustLine = "$valueLine`n`tPDZ_AdjustGunVisual(ctx.f1.f64, $axis);"
    if ($text.Contains($oldComment) -and $text.Contains($oldLoad)) {
        $text = $text.Replace($oldComment, $newComment).Replace($oldLoad, $newLoad)
        if (-not $text.Contains('PDZ_AdjustGunVisual(ctx.f1.f64,')) {
            $text = $text.Replace($valueLine, $adjustLine)
        }
        [IO.File]::WriteAllText($path, $text)
        Write-Host "[fix-mouse] decoupled gun sway in $($site[1])"
    } elseif ($text.Contains($newComment) -and $text.Contains($newLoad)) {
        if (-not $text.Contains("PDZ_AdjustGunVisual(ctx.f1.f64, $axis);")) {
            $text = $text.Replace($valueLine, $adjustLine)
            [IO.File]::WriteAllText($path, $text)
            Write-Host "[viewmodel] added gun visual offset to $($site[1])"
        }
    } else {
        throw "Generated gun-sway accessor $($site[1]) was not recognized."
    }
}

# Trace callers of the paired right-axis getter. This is the next stable point
# after the input snapshot and reveals the gameplay sensitivity/camera path.
$cameraReadFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.133.cpp'
if (Test-Path $cameraReadFile) {
    $text = [IO.File]::ReadAllText($cameraReadFile)
    $entry = "DEFINE_REX_FUNC(sub_8226C898) {`n`tREX_FUNC_PROLOGUE();"
    $trace = "$entry`n`tPDZ_TraceCameraRead(uint32_t(ctx.lr), ctx.r3.u32);"
    if ($text.Contains($entry) -and -not $text.Contains($trace)) {
        [IO.File]::WriteAllText($cameraReadFile, $text.Replace($entry, $trace))
        Write-Host '[trace-mouse] recording PDZ camera-axis reader callers'
    } elseif (-not $text.Contains($trace)) {
        throw 'Generated camera-axis reader trace site was not recognized.'
    }
}

# Widen perspective projection frusta without changing the display aspect.
# PDZ uses four closely related builders for 3x4 and 4x4 projection matrices.
$projectionSites = @(
    @('perfectdarkzerorecomp_recomp.29.cpp',  'sub_826F80D8'),
    @('perfectdarkzerorecomp_recomp.116.cpp', 'sub_826F8238'),
    @('perfectdarkzerorecomp_recomp.31.cpp',  'sub_8270BF18'),
    @('perfectdarkzerorecomp_recomp.33.cpp',  'sub_82260060')
)
foreach ($site in $projectionSites) {
    $path = Join-Path $GeneratedDir $site[0]
    if (-not (Test-Path $path)) { continue }
    $text = [IO.File]::ReadAllText($path)
    $marker = "DEFINE_REX_FUNC($($site[1])) {`n`tREX_FUNC_PROLOGUE();"
    $call = "$marker`n`tPDZ_AdjustProjectionFrustum(ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64, ctx.f5.f64, ctx.f6.f64, uint32_t(ctx.lr));"
    $oldCall = "$marker`n`tPDZ_AdjustProjectionFrustum(ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64);"
    if ($text.Contains($oldCall)) {
        $text = $text.Replace($oldCall, $call)
        [IO.File]::WriteAllText($path, $text)
        Write-Host "[fix-fov] upgraded perspective builder $($site[1])"
        continue
    }
    if ($text.Contains($marker) -and -not $text.Contains($call)) {
        [IO.File]::WriteAllText($path, $text.Replace($marker, $call))
        Write-Host "[fix-fov] patched perspective builder $($site[1])"
    } elseif (-not $text.Contains($call)) {
        throw "Generated FOV patch site $($site[1]) was not recognized."
    }
}

# TU0 startup video: retain startup notifications, flag restoration, and the
# normal null-video completion path into state 5. Do not bypass initialization.
$introFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.56.cpp'
$text = [IO.File]::ReadAllText($introFile)
$introMarker = "`t// beq 0x826173c0`n`tif (ctx.cr0.eq) goto loc_826173C0;"
$introPatch = "`t// PDZ startup video skip; continue through normal state setup and cleanup.`n`tif (rex::cvar::Query<bool>(`"pdz_skip_intro`")) goto loc_826173EC;`n$introMarker"
if (-not $text.Contains($introPatch)) {
    if (-not $text.Contains($introMarker)) { throw 'Startup intro skip site not recognized.' }
    [IO.File]::WriteAllText($introFile, $text.Replace($introMarker, $introPatch))
    Write-Host '[intro] added optional startup video bypass'
}

# Frontend state 2 plays intro2; state 3 and mission videos remain untouched.
$frontendFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.158.cpp'
$text = [IO.File]::ReadAllText($frontendFile)
$frontendMarker = "`t// bl 0x823975c0`n`tctx.lr = 0x822262B4;"
$frontendPatch = "`t// Skip intro2 and the following attract video. Advance to state 3 so its`n`t// existing no-video path performs the normal frontend/title initialization.`n`tif (ctx.r3.u32 == 0x82098800u && rex::cvar::Query<bool>(`"pdz_skip_intro`")) {`n`t`tREX_STORE_U32(ctx.r27.u32, 3);`n`t`tgoto loc_8222763C;`n`t}`n$frontendMarker"
if (-not $text.Contains($frontendPatch)) {
    $oldFrontendPatch = "`t// Skip only the intro2 asset; state 2's existing null-video path advances normally.`n`tif (ctx.r3.u32 == 0x82098800u && rex::cvar::Query<bool>(`"pdz_skip_intro`")) goto loc_8222763C;`n$frontendMarker"
    if ($text.Contains($oldFrontendPatch)) {
        $text = $text.Replace($oldFrontendPatch, $frontendPatch)
    } elseif ($text.Contains($frontendMarker)) {
        $text = $text.Replace($frontendMarker, $frontendPatch)
    } else { throw 'Frontend intro2 site not recognized.' }
    [IO.File]::WriteAllText($frontendFile, $text)
    Write-Host '[intro] added optional frontend intro2 bypass'
}

# Only the two time-scaled walking velocity blends in sub_8259EBD0. Do not
# modify the shared power helper or the camera's similar acceleration routines.
$movementFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.21.cpp'
$text = [IO.File]::ReadAllText($movementFile).Replace("`r`n", "`n")
if (-not $text.Contains('#include "../../src/pdz_qol.h"')) {
    $include = '#include "perfectdarkzerorecomp_funcs.21.h"'
    if (-not $text.Contains($include)) { throw 'Movement include site not recognized.' }
    $text = $text.Replace($include, "$include`n#include `"../../src/pdz_qol.h`"")
}
$axis = 0
foreach ($returnAddress in '8259EFAC', '8259F068') {
    $marker = "`tctx.lr = 0x$returnAddress;`n`tsub_82212A40(ctx, base);"
    $oldPatch = "$marker`n`tctx.f1.f64 = PDZ_AdjustMovementRetention(base, ctx.r31.u32, ctx.f1.f64, $axis);"
    $previousPatch = "$marker`n`tctx.f1.f64 = PDZ_AdjustMovementRetention(base, ctx.r31.u32, ctx.f1.f64, $axis, ctx.f23.f64);"
    $target = if ($axis -eq 0) { 'ctx.f24.f64' } else { 'ctx.f14.f64' }
    $patch = "$marker`n`tctx.f1.f64 = PDZ_AdjustMovementRetention(base, ctx.r31.u32, ctx.f1.f64, $axis, ctx.f23.f64, ctx.f27.f64 * ctx.f23.f64 / ctx.f15.f64, $target);"
    $text = $text.Replace($oldPatch, $patch).Replace($previousPatch, $patch)
    if (-not $text.Contains($patch)) {
        if (-not $text.Contains($marker)) { throw "Walking blend $returnAddress not recognized." }
        $text = $text.Replace($marker, $patch)
    }
    $axis++
}
# Scale requested displacements, not accumulated velocities or global limits.
# Axis 0 updates the forward timer exactly once; axis 1 uses the same scale.
foreach ($site in @(@('8259EF70','f24',0), @('8259F038','f14',1))) {
    $marker = "loc_$($site[0]):"
    $patch = "$marker`n`tctx.$($site[1]).f64 *= PDZ_ClassicTargetScale(base, ctx.r31.u32, ctx.r29.u32, ctx.f23.f64, $($site[2]));"
    if (-not $text.Contains($patch)) {
        if (-not $text.Contains($marker)) { throw "Classic target $($site[0]) not recognized." }
        $text = $text.Replace($marker, $patch)
    }
}
# Use the same scale on the directional cap, otherwise diagonal movement can
# still hit the unscaled cap and defeat the requested 80% baseline.
$capLine = '`tctx.f13.f64 = double(float(std::fma(ctx.f12.f64, ctx.f13.f64, ctx.f29.f64)));'.Replace('`t', "`t")
$capMarker = "$capLine`n`t// fmuls f12,f0,f13"
$capPatch = "$capLine`n`tctx.f13.f64 *= PDZ_ClassicTargetScale(base, ctx.r31.u32, ctx.r29.u32, ctx.f23.f64, 1);`n`t// fmuls f12,f0,f13"
if (-not $text.Contains($capPatch)) {
    if (([regex]::Matches($text, [regex]::Escape($capMarker))).Count -ne 1) {
        throw 'Classic directional cap site not uniquely recognized.'
    }
    $text = $text.Replace($capMarker, $capPatch)
}
if ($text -ne [IO.File]::ReadAllText($movementFile).Replace("`r`n", "`n")) {
    [IO.File]::WriteAllText($movementFile, $text)
    Write-Host '[movement] reapplied local walking response hooks'
}

# Normalize only the equipment selection in the local walking getter. Its
# hit-timer speed substitution is also bypassed only for Classic walking.
# Damage/timer state, other callers and other movement limits remain unchanged.
$equipmentFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.160.cpp'
$text = [IO.File]::ReadAllText($equipmentFile).Replace("`r`n", "`n")
$include = '#include "perfectdarkzerorecomp_funcs.160.h"'
if (-not $text.Contains('#include "../../src/pdz_qol.h"')) {
    if (-not $text.Contains($include)) { throw 'Equipment include site not recognized.' }
    $text = $text.Replace($include, "$include`n#include `"../../src/pdz_qol.h`"")
}
$marker = "loc_8253ED78:`n"
$patch = "$marker`tctx.f1.f64 = PDZ_ClassicEquipmentFactor(base, ctx.r9.u32, REX_LOAD_U32(ctx.r1.u32 + 88), ctx.f1.f64);`n"
if (-not $text.Contains($patch)) {
    if (-not $text.Contains($marker)) { throw 'Equipment selection site not recognized.' }
    $text = $text.Replace($marker, $patch)
}
$hitMarker = "`tctx.f1.f64 = ctx.f13.f64;`nloc_8253ED90:"
$hitPatch = "`tctx.f1.f64 = ctx.f13.f64;`n`t// Classic ignores only the recent-hit speed substitution (timer +5712 < 2s).`n`tctx.f1.f64 = PDZ_ClassicEquipmentFactor(base, ctx.r9.u32, REX_LOAD_U32(ctx.r1.u32 + 88), ctx.f1.f64);`nloc_8253ED90:"
if (-not $text.Contains($hitPatch)) {
    if (-not $text.Contains($hitMarker)) { throw 'Recent-hit speed substitution site not recognized.' }
    $text = $text.Replace($hitMarker, $hitPatch)
}
if ($text -ne [IO.File]::ReadAllText($equipmentFile).Replace("`r`n", "`n")) {
    [IO.File]::WriteAllText($equipmentFile, $text)
    Write-Host '[movement] reapplied Classic equipment and recent-hit speed normalization'
}

# Scale final NPC health loss only; the subsequent armor-loss calculation and
# native invulnerability/death handling are left intact. Host helper checks the
# NPC vtable implementations, excluding the player and non-character objects.
$npcFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.179.cpp'
$text = [IO.File]::ReadAllText($npcFile).Replace("`r`n", "`n")
$include = '#include "perfectdarkzerorecomp_funcs.179.h"'
if (-not $text.Contains('#include "../../src/pdz_qol.h"')) {
    if (-not $text.Contains($include)) { throw 'NPC health include site not recognized.' }
    $text = $text.Replace($include, "$include`n#include `"../../src/pdz_qol.h`"")
}
$marker = "`tif (!ctx.cr6.gt) goto loc_825373A4;`n"
$patch = "$marker`tctx.f30.f64 = PDZ_ClassicNpcHealthLoss(base, ctx.r31.u32, ctx.f30.f64);`n"
if (-not $text.Contains($patch)) {
    if (-not $text.Contains($marker)) { throw 'NPC health-loss site not recognized.' }
    $text = $text.Replace($marker, $patch)
}
if ($text -ne [IO.File]::ReadAllText($npcFile).Replace("`r`n", "`n")) {
    [IO.File]::WriteAllText($npcFile, $text)
    Write-Host '[combat] reapplied optional NPC-only final health-loss hook'
}

# Remove the failed walking-request hook, including on already generated trees.
$retiredFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.170.cpp'
$retired = [IO.File]::ReadAllText($retiredFile)
$clean = $retired.Replace("`tPDZ_ApplyDodgeRollVelocity(base, ctx.r3.u32, ctx.lr);`r`n", '').Replace("`tPDZ_ApplyDodgeRollVelocity(base, ctx.r3.u32, ctx.lr);`n", '')
if ($clean -ne $retired) { [IO.File]::WriteAllText($retiredFile, $clean) }
# Adjust only the world-space horizontal per-frame root displacement in the
# ordinary animation-motion integrator. Native vertical/timing paths remain.
foreach ($route in @(@('10', '82685188', 'r31', 'r29', 'f31', '0'),
                      @('72', '826872F8', 'r31', 'r30', 'f30', '1'))) {
    $routeFile = Join-Path $GeneratedDir "perfectdarkzerorecomp_recomp.$($route[0]).cpp"
    $oldRoute = [IO.File]::ReadAllText($routeFile)
    $routeText = $oldRoute.Replace("`r`n", "`n")
    $routeInclude = "#include `"perfectdarkzerorecomp_funcs.$($route[0]).h`""
    if (-not $routeText.Contains('#include "../../src/pdz_qol.h"')) {
        if (-not $routeText.Contains($routeInclude)) { throw 'Dodge route include missing.' }
        $routeText = $routeText.Replace($routeInclude, "$routeInclude`n#include `"../../src/pdz_qol.h`"")
    }
    $routeBegin = $routeText.IndexOf("DEFINE_REX_FUNC(sub_$($route[1])) {")
    if ($routeBegin -lt 0) { throw 'Dodge route function missing.' }
    $routeEnd = $routeText.IndexOf('DEFINE_REX_FUNC(', $routeBegin + 1)
    if ($routeEnd -lt 0) { throw 'Dodge route function boundary missing.' }
    $routeBody = $routeText.Substring($routeBegin, $routeEnd - $routeBegin)
    $anchor = if ($route[0] -eq '10') { "ctx.r29.u64 = ctx.r4.u64;`n" } else { "ctx.r31.u64 = ctx.r3.u64;`n" }
    $call = "`tPDZ_TraceDodgeRoute(base, ctx.$($route[2]).u32, ctx.$($route[3]).u32, ctx.$($route[4]).f64, $($route[5]));`n"
    # Fatigue observation is gameplay, independent of optional route diagnostics.
    if ($route[0] -eq '10') {
        $observe = "`tPDZ_ObserveDodgeRoll(base, ctx.r31.u32, ctx.r29.u32, ctx.f31.f64);`n"
        if (-not $routeBody.Contains($observe)) {
            if (-not $routeBody.Contains($anchor)) { throw 'Dodge observation anchor missing.' }
            $routeBody = $routeBody.Replace($anchor, "$anchor$observe")
        }
    }
    if (-not $routeBody.Contains($call)) {
        if (-not $routeBody.Contains($anchor)) { throw 'Dodge route argument capture missing.' }
        $traceAnchor = if ($route[0] -eq '10') { "$anchor$observe" } else { $anchor }
        $routeBody = $routeBody.Replace($traceAnchor, "$traceAnchor$call")
    }
    # Splice once using indices into the original text. Splicing after observe
    # insertion and then reusing the old end index corrupted the next function
    # on genuinely fresh output (existing adapted fixtures concealed it).
    $routeText = $routeText.Substring(0, $routeBegin) + $routeBody + $routeText.Substring($routeEnd)
    if ($route[0] -eq '10') {
        foreach ($probe in @(@("if (ctx.cr0.eq) goto loc_82685364;`n",4), @("loc_82685364:`n",3))) {
            $probeCall="`tPDZ_TraceDodgeRoute(base, ctx.r31.u32, ctx.r29.u32, ctx.f31.f64, $($probe[1]));`n"
            if(-not $routeText.Contains($probeCall)) {
                if(-not $routeText.Contains($probe[0])) { throw 'Dodge movement route diagnostic anchor missing.' }
                $routeText=$routeText.Replace($probe[0],"$($probe[0])$probeCall")
            }
        }
    }
    if ($routeText -ne $oldRoute.Replace("`r`n", "`n")) { [IO.File]::WriteAllText($routeFile, $routeText) }
}
$dodgeFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.158.cpp'
$old = [IO.File]::ReadAllText($dodgeFile)
$text = $old.Replace("`r`n", "`n")
$include = '#include "perfectdarkzerorecomp_funcs.158.h"'
if (-not $text.Contains('#include "../../src/pdz_qol.h"')) {
    if (-not $text.Contains($include)) { throw 'Dodge roll include site not recognized.' }
    $text = $text.Replace($include, "$include`n#include `"../../src/pdz_qol.h`"")
}
$begin = $text.IndexOf('DEFINE_REX_FUNC(sub_82687DF8) {')
if ($begin -lt 0) { throw 'Dodge roll root integrator not recognized.' }
$end = $text.IndexOf('DEFINE_REX_FUNC(', $begin + 1)
if ($end -lt 0) { throw 'Dodge roll integrator boundary not recognized.' }
$body = $text.Substring($begin, $end - $begin)
$marker = "loc_8268805C:`n"
$patch = "$marker`tPDZ_AdjustDodgeRollDelta(base, ctx.r28.u32, ctx.r31.u32, ctx.f28.f64, ctx.f29.f64, ctx.f30.f64);`n"
if (-not $body.Contains($patch)) {
    if (-not $body.Contains($marker)) { throw 'Dodge roll world displacement site not recognized.' }
    $body = $body.Replace($marker, $patch)
    $text = $text.Substring(0, $begin) + $body + $text.Substring($end)
}
if ($text -ne $old.Replace("`r`n", "`n")) {
    [IO.File]::WriteAllText($dodgeFile, $text)
    Write-Host '[movement] reapplied optional local-player dodge roll root displacement hook'
}

# The local player's ordinary rolls take884C8, not87DF8. Boost the temporary
# world velocity before position integration and nativeC7F0/CD50 collision.
# Remove the multiplier from collision-resolved velocity feedback so it cannot
# compound on subsequent frames. Never change persistent animation time or Y.
$rollFile = Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.136.cpp'
$rollOld = [IO.File]::ReadAllText($rollFile).Replace("`r`n", "`n")
$rollText = $rollOld
if (-not $rollText.Contains('#include "../../src/pdz_qol.h"')) {
    $rollText = $rollText.Replace('#include "perfectdarkzerorecomp_funcs.136.h"', '#include "perfectdarkzerorecomp_funcs.136.h"' + "`n#include `"../../src/pdz_qol.h`"")
}
$rollBegin = $rollText.IndexOf('DEFINE_REX_FUNC(sub_826884C8) {')
$rollEnd = $rollText.IndexOf('DEFINE_REX_FUNC(', $rollBegin + 1)
if ($rollBegin -lt 0 -or $rollEnd -lt 0) { throw 'Alternate roll integrator missing.' }
$rollBody = $rollText.Substring($rollBegin, $rollEnd - $rollBegin)
if (-not $rollBody.Contains('double pdz_roll_scale = 1.;')) {
    $rollBody = $rollBody.Replace("REX_FUNC_PROLOGUE();`n", "REX_FUNC_PROLOGUE();`n`t double pdz_roll_scale = 1.;`n")
    $boostAnchor = "REX_STORE_U32(ctx.r28.u32 + 428, temp.u32);`n"
    $boost = @'
	// Optional horizontal roll boost; collision and vertical movement stay native.
	pdz_roll_scale = PDZ_AdjustDodgeRollVelocity(base, ctx.r29.u32, ctx.r28.u32,
	                                           ctx.f31.f64, ctx.f30.f64, ctx.f28.f64);
'@
    $feedbackAnchor = "sub_82626C58(ctx, base);`n"
    $feedback = "`tctx.f30.f64 /= pdz_roll_scale;`n`tctx.f28.f64 /= pdz_roll_scale;`n"
    $restoreAnchor = "loc_82688884:`n"
    $restore = "`tctx.f30.f64 *= pdz_roll_scale;`n`tctx.f28.f64 *= pdz_roll_scale;`n"
    foreach ($anchor in @($boostAnchor, $feedbackAnchor, $restoreAnchor)) {
        if (-not $rollBody.Contains($anchor)) { throw 'Alternate roll movement anchor missing.' }
    }
    $rollBody = $rollBody.Replace($boostAnchor, $boostAnchor + $boost + "`n")
    $rollBody = $rollBody.Replace($feedbackAnchor, $feedbackAnchor + $feedback)
    $rollBody = $rollBody.Replace($restoreAnchor, $restoreAnchor + $restore)
    $rollText = $rollText.Substring(0, $rollBegin) + $rollBody + $rollText.Substring($rollEnd)
}
if ($rollText -ne $rollOld) { [IO.File]::WriteAllText($rollFile, $rollText) }

# Capture the requested action before virtual weapon-specific remapping. Adjust
# the selected playback rate, not dt, blend duration, clip IDs or event dispatch.
# Capture SDK-delivered raw input (including Steam Input) before consuming
# horizontal D-pad in the returned copy during LT-held gameplay. Preserve LT
# and all other buttons; raw D-pad remains available for scope interpolation.
foreach ($zoomSite in @(@('143', '82AC4CB8'), @('129', '8259CC00'))) {
    $zoomFile = Join-Path $GeneratedDir "perfectdarkzerorecomp_recomp.$($zoomSite[0]).cpp"
    $oldZoom = [IO.File]::ReadAllText($zoomFile)
    $zoomText = $oldZoom.Replace("`r`n", "`n")
    $zoomInclude = "#include `"perfectdarkzerorecomp_funcs.$($zoomSite[0]).h`""
    if (-not $zoomText.Contains('#include "../../src/pdz_qol.h"')) {
        if (-not $zoomText.Contains($zoomInclude)) { throw 'Zoom include missing.' }
        $zoomText = $zoomText.Replace($zoomInclude, "$zoomInclude`n#include `"../../src/pdz_qol.h`"")
    }
    $zoomBegin = $zoomText.IndexOf("DEFINE_REX_FUNC(sub_$($zoomSite[1])) {")
    if ($zoomBegin -lt 0) { throw 'Zoom function missing.' }
    $zoomEnd = $zoomText.IndexOf('DEFINE_REX_FUNC(', $zoomBegin + 1)
    if ($zoomEnd -lt 0) { throw 'Zoom boundary missing.' }
    $zoomBody = $zoomText.Substring($zoomBegin, $zoomEnd - $zoomBegin)
    if ($zoomSite[0] -eq '143') {
        $entry = "`tREX_FUNC_PROLOGUE();`n"
        $capture = "$entry`tconst uint32_t pdz_input_state = ctx.r4.u32, pdz_input_user = ctx.r3.u32;`n"
        if (-not $zoomBody.Contains($capture)) { $zoomBody = $zoomBody.Replace($entry, $capture) }
        $marker = "`t__imp__XamInputGetState(ctx, base);`n"
        $patch = "$marker`tPDZ_RecordWeaponInput(base, pdz_input_state, pdz_input_user, ctx.r3.u32);`n"
    } else {
        $entry = "`tREX_FUNC_PROLOGUE();`n"
        $capture = "$entry`tbool pdz_scope_allowed = false;`n"
        if (-not $zoomBody.Contains($capture)) { $zoomBody = $zoomBody.Replace($entry, $capture) }
        # Suppress scope intent at this player's scope updater only. Preserve
        # LT in XInput and all native left-hand weapon/grenade fire dispatch.
        $scopeEntry="`tctx.lr = 0x8259CC5C;`n`tsub_8253E380(ctx, base);`n"
        $scopeEntryPatch="$scopeEntry`tctx.r3.u64 = PDZ_ScopeAllowed(base, ctx.r31.u32, ctx.r3.u32);`n"
        if(-not $zoomBody.Contains($scopeEntryPatch)) {
            if(-not $zoomBody.Contains($scopeEntry)) { throw 'Scope entry gate missing.' }
            $zoomBody=$zoomBody.Replace($scopeEntry,$scopeEntryPatch)
        }
        $gate = "`tctx.lr = 0x8259CFF8;`n`tsub_8253E380(ctx, base);`n"
        $gateOld = "$gate`t pdz_scope_allowed = (ctx.r3.u32 & 255u) != 0;`n"
        $gatePatch = "$gate`tctx.r3.u64 = PDZ_ScopeAllowed(base, ctx.r31.u32, ctx.r3.u32);`n`t pdz_scope_allowed = (ctx.r3.u32 & 255u) != 0;`n"
        if($zoomBody.Contains($gateOld)) { $zoomBody=$zoomBody.Replace($gateOld,$gatePatch) }
        if (-not $zoomBody.Contains($gatePatch)) {
            if (-not $zoomBody.Contains($gate)) { throw 'Native scope gate missing.' }
            $zoomBody = $zoomBody.Replace($gate, $gatePatch)
        }
        $marker = "loc_8259D004:`n"
        $patch = "$marker`t{`n`t`tPPCRegister pdz_zoom{};`n`t`tpdz_zoom.u32 = REX_LOAD_U32(ctx.r31.u32 + 6816);`n`t`tpdz_zoom.f32 = float(PDZ_DpadZoomPosition(base, ctx.r31.u32, pdz_zoom.f32, ctx.f27.f64, pdz_scope_allowed));`n`t`tREX_STORE_U32(ctx.r31.u32 + 6816, pdz_zoom.u32);`n`t}`n"
    }
    if (-not $zoomBody.Contains($patch)) {
        if (-not $zoomBody.Contains($marker)) { throw 'Zoom hook site missing.' }
        $zoomBody = $zoomBody.Replace($marker, $patch)
    }
    $zoomText = $zoomText.Substring(0, $zoomBegin) + $zoomBody + $zoomText.Substring($zoomEnd)
    if ($zoomText -ne $oldZoom.Replace("`r`n", "`n")) { [IO.File]::WriteAllText($zoomFile, $zoomText) }
}
# Both immediate and blended playback paths return this same rate to callers.
foreach ($site in @(@('166', '82234558', '822346D8', 'r30', '822346F4', '82230330'),
                    @('126', '82234B60', '82234BB4', 'r31', '82234BDC', '82230570'))) {
    $weaponFile = Join-Path $GeneratedDir "perfectdarkzerorecomp_recomp.$($site[0]).cpp"
    $old = [IO.File]::ReadAllText($weaponFile)
    $text = $old.Replace("`r`n", "`n")
    $include = "#include `"perfectdarkzerorecomp_funcs.$($site[0]).h`""
    if (-not $text.Contains('#include "../../src/pdz_qol.h"')) {
        if (-not $text.Contains($include)) { throw 'Weapon rate include site not recognized.' }
        $text = $text.Replace($include, "$include`n#include `"../../src/pdz_qol.h`"")
    }
    $entry = "DEFINE_REX_FUNC(sub_$($site[1])) {`n`tREX_FUNC_PROLOGUE();"
    $capture = "$entry`n`tconst uint32_t pdz_requested_weapon_action = ctx.r4.u32;"
    if (-not $text.Contains($capture)) {
        if (-not $text.Contains($entry)) { throw 'Weapon rate entry not recognized.' }
        $text = $text.Replace($entry, $capture)
    }
    $marker = "`tctx.lr = 0x$($site[2]);`n`tsub_82234C08(ctx, base);"
    $patch = "$marker`n`t{`n`t`tPPCRegister pdz_rate{};`n`t`tpdz_rate.u32 = REX_LOAD_U32(ctx.r1.u32 + 88);`n`t`tpdz_rate.f32 = float(PDZ_WeaponActionRate(base, ctx.$($site[3]).u32, pdz_requested_weapon_action, pdz_rate.f32));`n`t`tREX_STORE_U32(ctx.r1.u32 + 88, pdz_rate.u32);`n`t}"
    if (-not $text.Contains($patch)) {
        if (-not $text.Contains($marker)) { throw 'Weapon rate call site not recognized.' }
        $text = $text.Replace($marker, $patch)
    }
    $actionMarker = "`tctx.lr = 0x$($site[4]);`n`tsub_$($site[5])(ctx, base);"
    $actionPatch = "$actionMarker`n`tPDZ_ObserveUnarmedAction(base, ctx.$($site[3]).u32, pdz_requested_weapon_action, ctx.r3.u32);"
    if (-not $text.Contains($actionPatch)) {
        if (-not $text.Contains($actionMarker)) { throw 'Unarmed action completion site missing.' }
        $text = $text.Replace($actionMarker,$actionPatch)
    }
    if ($text -ne $old.Replace("`r`n", "`n")) {
        [IO.File]::WriteAllText($weaponFile, $text)
        Write-Host "[weapons] reapplied playback rate hook $($site[1])"
    }
}

