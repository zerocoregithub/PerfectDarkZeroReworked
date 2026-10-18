# Forking and upstream integration

## Current publication decision — 18 October 2026

The maintainer selected a clean source-only repository with sibling game and SDK
folders. Upstream Git history is not imported because its artwork history has not
been cleared for redistribution. Attribution, licences and reference identifiers
are retained. This is not a literal shared-ancestry fork; the source-delta CSVs aid
review but are not independently tested patches. The advice below describes the
historical options and integration boundaries, not a requirement to import history.

The public package preserves exact source bytes using appended Git attribute rules
and refreshed integrity manifests. See the umbrella VERIFYING.md. Original vendor
attribute rules/notices are retained; this is packaging, not vendor code changes.

## Provenance, not an invented patch history

The current private game and SDK workspaces are exported snapshots: both Git
repositories have no commits. They are not already clean forks with a reviewable
commit series. Publishing the whole workspace or inventing commits attributed to
upstream would obscure provenance and risks exposing private/game-derived data.

Recovered historical bases are game99ddda11c40d5d605f9b669310f0cb396e63194e
(`V1.3 Third Person Camera mod`) and SDKbd833a2ab69d305f99ddf57418fc38ffec8af93d.
The earlier locally reviewed upstream update was
4cda209a6e13593c4caca0987b5aa44221115f3d (local profile manager merge).
These are historical reference points, not a claim about today's remote HEAD or
that the working tree equals any one commit. Exact export manifests identify
modified content. Runtime SDK base is not the pinned legacy compiler revision.

Before GitHub publication, choose the intended upstream fork base and inspect its
history/content for distribution suitability. In a new staging repository, overlay
only reviewed source, modified SDK work and docs from explicit manifests. Record
the differences against the chosen base, retain notices and review the final
staging tree. Do not rewrite or push the private workspaces during that step.

## Suggested change series

| Series | Dependencies / acceptance |
|---|---|
| Reproducible build and generator adapters | Pin supported executable/compiler/partition; quiet build, repeat insertion fixture, no generated guest code committed |
| Generic runtime fixes | Silent audio pacing, queue correctness and quiet logging; independent harnesses, no guest-address policy in generic fixes |
| SDK controller/motion primitives | Device provenance, capability discovery, non-consuming samples, UI blocking, rumble metadata; host/API and hardware tests together |
| Game input policy | Bindings/shortcuts, independent gyro/stick tuning, rumble-aware filtering, Locktopus; needs matching SDK input interfaces |
| Optional gameplay helpers | Reload-first interaction, movement/dodge and aim policy; pure tests plus native hook/ownership tests |
| HUD/settings/portable launcher | Snapshot contracts, preset/autosave schema, first-run Vanilla and session reset, save/relocation/exit tests |
| Third-person enhancements | Retain upstream camera credit; native-eye clearance, impact marker and scoped-camera tests, no claim of body-pose fix |
| Experimental rendering/cover | Separate opt-in series; match SDK shader/camera changes and preserve the unmodified path |

This is a proposed decomposition, not an already independently cherry-pickable
series. Some game hooks and SDK renderer policy are currently coupled. An upstream
maintainer may want only the generic API/fixes and different game defaults.

## Coupling to resolve deliberately

- SDK SDL driver currently queries pdz_gyro_mode/pdz_gyro_steam_driver. Extracting
  a reusable motion API should move that policy behind a generic device/sensor
  configuration interface; do not merely rename flags or remove enable/keepalive.
- GameCube/blur/scene-grading/HDR touches SDK renderer/presentation. Game shaders
  and fingerprints are not a generic renderer upgrade. Keep generic output plumbing
  separate from game-specific recognition and material policy.
- Third person publishes the native-eye snapshot consumed by pdz_qol clearance.
  The overlay consumes impact/view snapshots. Carry those contracts together.
- Native mouse conditioning has two implementations in the tree; only the app's
  path is compiled. Do not also enable game_patches.cpp when taking upstream updates.
- CVar definitions, startup initialization, presets, capture and input persistence
  span several files. Change a setting through the complete lifecycle, not one UI.
- The current generated adapter is a large, ordered integration layer. Extract
  per-feature adapters only after preserving source checks and fresh/repeated-run
  equivalence; file numbers/register positions are not an upstream-stable API.

Do not copy the runtime DLL into an unrelated upstream binary. InputDriver virtual
methods and host/renderer exports require a matched build. ABI compatibility is
not established by a matching DLL filename or ReXGlue version string.

## Gate for each integration change

1. Record source base, modified files, dependencies and supported executable.
2. Preserve original attribution and exclude proprietary/private inputs and outputs.
3. Demonstrate disabled/native behaviour as well as the enabled feature.
4. Test pure policy, native hook placement, shared-state/capture resets and matched
   SDK imports. State missing hardware/gameplay verification explicitly.
5. Keep settings/save migration non-destructive and preserve the accepted rollback.
6. Seek agreement before feature removal, architectural splitting or upstream PRs.

The previously prepared release ZIPs/source archives remain frozen evidence. New
comments/docs or changes require a new reviewed export manifest before delivery;
the older manifest will not automatically include this guide or AGENTS.md.
