# Perfect Dark Zero Reworked — v0.9.0-rc1

An optional controls, gameplay and presentation enhancement fork of
[nikolaygorb's PerfectDarkZeroRecomp](https://github.com/nikolaygorb/PerfectDarkZeroRecomp),
built with the modified ReXGlue SDK. Third-person mode and the underlying PC
recompilation originate upstream; Reworked expands their controls and behaviour.
Not affiliated with Microsoft or Rare. No game ISO or extracted assets supplied.

**Release candidate, not yet approved for public upload.** See
[known issues](KNOWN_ISSUES.md). Experimental features are optional.

## Installation

Windows x64 / DX12 is the primary tested setup. Extract the entire portable
package into a writable local folder. Keep the launcher beside Data and ROM.
Place your own legally obtained supported ISO in ROM and run
`PerfectDarkZeroReworked-v0.9.0-rc1.exe`. One supported ISO is selected automatically;
otherwise choose its location. Approve local extraction and allow it to finish.
Nothing downloads game data or modifies your ISO. Later starts reuse extracted data.

Supported input is the US/NTSC-U executable with CRC32 `66DC74B6` (Media ID
`41EEAFCB`). USA Rev1 and other executable revisions are not supported. A filename
or matching region alone does not establish compatibility. Renaming an ISO won't
make a different revision compatible.

Settings and saves live under Data. Back up the entire portable folder before
upgrading; do not replace your settings/saves with another person's files. Use
the root launcher, not the executable inside Data. Fresh setup starts Vanilla,
normal Y, intros shown and first person. Intro skip is optional in the overlay.

## Controls

| Action | Keyboard / controller |
|---|---|
| Enhancement overlay | F8 / View+Menu (Select+Start) |
| Change perspective | F7 / both stick clicks together |
| Third-person shoulder | F6 / right-stick click |
| Cycle display mode | F11 |
| GameCube visual toggle | Semicolon / supported native Quick Access button |

Overlay: D-pad/left stick navigates, bottom face selects, right face cancels,
shoulders change tabs. Exit and Restart are available in the overlay. Gameplay
bindings are configurable. X reload-first interaction and held-A cover are optional
enhancement behaviours; built-in Vanilla retains original gameplay behaviour.

## Presets and customization

Vanilla Perfect Dark Zero keeps original gameplay/presentation with PC input
options. Classic Rare Style Perfect Dark supplies classic-inspired movement,
free aim/weapon sway, HUD/scopes and tuning. Modernized Perfect Dark Zero uses
the enhanced Classic configuration without added weapon sway and with native
rather than custom aim assistance. Classic/Modernized select Relaxed stick
response; Vanilla selects Original. Look preferences are preserved when switching.
GameCube graphics are a separate experimental option. Three user slots and autosave
are included; settings errors are reported rather than silently accepted.

Expanded controls include mouse look/rebinding, native SDL3 gyro, response curves,
deadzone/ADS tuning, rumble intensity and rumble-aware gyro filtering. Motion input
requires a controller/connection that exposes sensors; virtual Xbox pads have none.
Steam Controller and DualShock4 have user playtest evidence; other supported SDL
sensor paths are not a complete hardware certification. See known limitations.

Other optional enhancements include Classic HUD/scopes, zoom-scaled free-aim edge
turning, X reload-first interaction, fixed dodge fatigue, Locktopus mouse controls,
FOV/weapon-bob/action tuning, blur suppression, SMAA and colour grading.
Third-person shoulder transitions, wall-clearance targeting and dynamic impact
indication expand upstream third person. Scoped third person temporarily returns
to the native eye view, restoring the selected shoulder on release.

Experimental cover-camera stabilization and flatter GameCube-inspired shading are
not universal fixes. Windows/DX12 HDR output presents an SDR-derived image through
scRGB; it is not reconstructed scene-linear HDR lighting. No performance gain or
full platform/controller compatibility is promised.

## Reports

Include version, controller/connection, Steam state, GPU/driver/backend, preset,
mission/checkpoint and exact repeatable steps. Do not post your ISO, extracted
assets or personal saves. Runtime logging is disabled in normal release candidates;
do not expect a diagnostic log to be produced. No telemetry upload is performed.

## Source builds and attribution

Source generation needs the matched modified SDK and legally supplied game input.
The prebuilt first-run launcher extracts data; it does not compile source. Current
isolated fresh-source and delivered-archive builds have passed; gameplay acceptance
remains a separate check. See [build instructions](BUILDING.md).
Do not assume upstream SDK alone reproduces this fork. Original project MIT and
ReXGlue/dependency notices must be retained. Corresponding source must accompany
binary delivery; this document is not licence clearance.
