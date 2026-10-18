# Architecture and ownership

This is a source recompilation, not a new game engine. Original game behaviour
runs in generated guest C++ against ReXGlue; Reworked adds host-side policy at
identified call sites. Source paths below refer to this game repository unless
prefixed with `SDK/`, the separately matched sibling SDK source tree.

## Entry and data flow

```text
portable launcher -> first-run ISO extraction -> Data/game executable
  main.cpp -> PerfectdarkzerorecompApp -> runtime + generated guest code
  physical input -> SDK InputSystem/SDL -> pdz_input policy -> guest hooks
  guest hooks -> pdz_qol state + pure helpers -> native game results
  guest/render snapshots -> pdz_overlay HUD/settings -> CVar/config changes
```

The launcher derives Data/ROM from its own location. It is not source compilation
on a player's first launch. User-owned supported game assets are required; the
public source and clean payload do not contain those assets.

## Component map

| Owner | Responsibilities / integration entry |
|---|---|
| `main.cpp`, `perfectdarkzerorecomp_app.h` | App lifecycle, CVar definitions, startup settings layering, portable paths, narrowly guarded native fallback thunk; one app owns mouse policy |
| `pdz_input.cpp/.h` | Physical player-one routing, bindings, menu shortcuts, gyro activation/calibration/filtering, independent stick turn conversion |
| `pdz_qol.cpp/.h` | Guest-facing adapters, owner/type validation, HUD/aim/weapon/cover state; historically broad, not a stable library API |
| `pdz_*math.h`, policy headers | Small arithmetic/state machines, mostly independent of guest execution; tested by tools/test-release-pure.ps1 |
| `pdz_overlay.cpp/.h` | F8/controller menu, visual HUD/scopes/impact marker, capture, setting changes and autosave; currently also refreshes live FOV and experimental model identity |
| `pdz_presets.h`, `pdz_settings_file.h` | Effective preset policy and recoverable settings-file replacement; input bindings and persistence allowlists also have their own owners |
| `mod/third_person.cpp` | Upstream camera foundation plus shoulder easing, scoped native-eye transition and native-eye snapshot for clearance |
| `render/gpu_wait_hook.cpp` | Calls original GPU-ring poll, changes only waiting policy; not a guest-FPS counter or removal of synchronization |
| `pdz_portable_launcher.cpp`, setup/restart helpers | Portable first-run setup, ISO identity/extraction, process launch and lifecycle |
| `tools/fix-unresolved-tail-jumps.ps1` + child adapters | Versioned repair/insertion sites in generated units; historical name understates scope |
| `SDK/include/rex/input`, `SDK/src/input` | Physical device ownership, motion history, extra controls, UI/guest blocking and rumble; current implementation also has PDZ-specific policy lookups |
| SDK audio/logging/graphics/UI | Silent audio pacing, quiet runtime, renderer grading/stylization/blur/HDR and display facilities; separately qualified dependencies |

## Guest integration contract

The supported TU0 executable, compiler source and partition seed are independent
versioned inputs; see codegen/README.md. Addresses such as the player root and
vtable identities are verified for that executable, not portable across ISOs.
Generated unit numbers are part of the current adapter contract. Never silently
regenerate with the runtime SDK's newer compiler.

Callbacks use guest virtual addresses and big-endian load/store helpers. A
range-only check does not prove a page is mapped. Reconstructed/optional pointers
need mapped-span and owner/type checks; even a successful query is not lifetime
synchronization. Native-call-site assumptions must be documented, not generalized
to arbitrary guest data. Failed optional recognition should retain native behaviour.

`REX_HOOK_RAW` wrappers call their corresponding `__imp__` original where shown.
Other features require insertion inside a generated function because the relevant
registers/intermediate values are only available there. Port the call site and
host callback together; linking only the helper is not implementation.

## State, threads and freshness

- SDK state lock serializes device assignment/polling. Filters can run during
  guest polls or UI polls; their sensor reads re-enter that lock. Game snapshot
  locks must not introduce the opposite lock ordering.
- `pdz_input.cpp` protects per-device snapshots with pad_mutex. UI reads snapshots
  while gameplay is neutralized. Motion history is non-consuming; the game keeps
  a timestamp watermark so render/UI polling does not steal or double-integrate it.
- Sensor values are angular rates; timestamp differences produce turn deltas.
  Gyro space/activation/filtering is game policy, independent of stick response.
- `pdz_qol.cpp` publishes HUD, scope, free-aim, cover and impact snapshots under
  their specific mutexes/atomics. Consumers use owner/time checks. These checks
  are required state validation, not redundant diagnostic overhead.
- The historically named aim_input_trace_mutex protects live sway state too.
  TraceCoverCamera and several Observe callbacks feed gameplay in quiet builds.
- UI capture has release/drain rules so closing a menu does not fire held inputs
  into the game. Locktopus deliberately treats its pause-like player flag
  differently, using its active native input consumer and short lease.

This is an ownership map, not proof that every guest-memory access is race-free.
The remaining mixed range/mapped-page assumptions need separate bounded review.

## Aim, cover and third person

Manual/free aim, display cursor smoothing, weapon visuals, native targeting and
camera composition are distinct. A visual interpolation must not silently change
spread, collision or muzzle origin. The third-person clearance check constrains
the target using the native eye and existing native collision collector; the
impact marker reflects that constraint. It is not a new projectile-origin model.

The cover option keeps world-target consistency through composed camera views;
do not reorder producer, ray alignment, collision and final-view observations.
Weapon-scoped third person eases to the native eye, then restores the shoulder.
Scoped aim-assist suppression uses ADS semantics, not merely a narrowed FOV.
Third-person upper-body free-aim animation remains unfinished; transparency and
cover-side switching are not supplied by this architecture.

## Settings and compatibility

Startup loads hardware, mapping and enhancement files in order, with fresh Vanilla
initialization and a final first-person session reset. Presets are not all look
settings; sensitivities and personal settings have preservation rules. Capture,
input persistence and startup loading are separate schemas that must agree.

`Preset::Modernized` derives Classic through Value() with explicit exceptions.
Only Classic/Vanilla base columns are stored; a frozen test covers all252 effective
preset values. The obsolete Modernized column was removed without changing outputs. Old ignored
stick/free-aim keys and legacy preset identifiers are config compatibility, not
evidence that removed features are still active. Do not remove them without migration.

GameCube shading and cover-camera stabilization are optional experiments. P9P
attachment hiding still has an experimental UI/compiled path but is not a verified
working feature; preserve that distinction in release notes and integration work.

## What is not an active target

CMake's explicit source list excludes competing upstream game_patches.cpp and old
P9P trace source. Test-only A/B/outline headers do not make those parked features
active. Historical diagnostics/research should not be mistaken for shipping code.
Conversely, compiling every source with a wildcard can reintroduce conflicting hooks.
