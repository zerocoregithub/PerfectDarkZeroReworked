# Perfect Dark Zero Reworked — source repository

Controls, gameplay and presentation enhancements built on
[PerfectDarkZeroRecomp](https://github.com/nikolaygorb/PerfectDarkZeroRecomp) and
[ReXGlue](https://github.com/rexglue/rexglue-sdk). This repository keeps the game
and its matched modified SDK together while separating their changes by directory.
Not affiliated with Microsoft or Rare. Preserve upstream authorship and notices.

**No Perfect Dark Zero game assets are included.** You must supply your own
legally owned supported ISO. Different executable revisions are not interchangeable.
The clean portable binary extracts local game data on first launch; source builds
require developer tools and supported game input. Nothing downloads game data.

## Watch the walkthrough

[**Watch: Perfect Dark Zero Reworked — features, controls and gameplay**](https://youtu.be/fIbKRaN6MFM)

A hands-on tour of the presets, controller/gyro settings, aiming, presentation
options and experimental features, followed by mission gameplay.
[Skip to gameplay at 18:00](https://www.youtube.com/watch?v=fIbKRaN6MFM&t=1080s).

Ready to try it? [Download the Windows x64 release](https://github.com/zerocoregithub/PerfectDarkZeroReworked/releases/tag/v0.9.0-rc1).

## Steam Controller: recommended Windows setup

For native controller and gyro input, the maintainer's tested launch options are:

1. **Without Steam:** fully exit Steam, including its system-tray/background
   process, then launch `PerfectDarkZeroReworked-v0.9.0-rc1.exe` directly.
2. **With Steam:** add that root launcher as a **non-Steam game** and launch it
   from your Steam library. Use a standard gamepad layout. Leave Steam's gyro
   output set to **None** and configure native gyro in the game's overlay, rather
   than combining Steam's **Gyro to Mouse/Joystick** output with native gyro.

In the overlay (`F8` or **View + Menu / Select + Start**), open **Gyro**. If the
Steam Controller's native motion sensor is not detected, enable **Native Steam
Controller driver (restart required)** and restart the game. Enable/tune native
gyro there once the physical motion-capable controller is detected. A virtual
Xbox controller alone does not expose gyro.

**Avoid launching the EXE directly while Steam is running in the background.**
That combination can add desktop mouse/keyboard or emulated-controller actions
alongside native input, causing double presses, unexpected actions or doubled
aiming. Steam Controller desktop fallback ("lizard mode") and Steam's desktop
layout are related troubleshooting concerns, not identical input paths. Reworked
does not universally suppress Steam's desktop mappings or synchronize its native
settings with Steam Input. If interference persists, fully exit Steam and retry
the native path. These Windows observations do not establish Steam Deck/Proton
feature parity; see [known limitations](PerfectDarkZeroRecomp-v13/KNOWN_ISSUES.md).

## Start here

Check source integrity with [verification instructions](VERIFYING.md) before
building. Git packaging preserves the reviewed bytes across checkout settings.

- [Game source and player guide](PerfectDarkZeroRecomp-v13/README.md)
- [Build instructions](PerfectDarkZeroRecomp-v13/BUILDING.md)
- [Architecture](PerfectDarkZeroRecomp-v13/docs/ARCHITECTURE.md)
- [Contributing](PerfectDarkZeroRecomp-v13/CONTRIBUTING.md)
- [Upstream integration](PerfectDarkZeroRecomp-v13/docs/UPSTREAM_INTEGRATION.md)
- [Known limitations](PerfectDarkZeroRecomp-v13/KNOWN_ISSUES.md)
- [SDK source](rexglue-sdk-v13/README.md)
- [Source provenance](SOURCE_PROVENANCE.md)

The sibling layout is intentional: build/test wrappers select the matching SDK
without absolute machine-specific paths. Its dependencies are delivered as source,
not unresolved Git submodules. SOURCE_ARCHIVE.json verifies their exact contents;
the game build wrapper selects source-archive mode automatically. Do not replace
the modified SDK with an arbitrary upstream checkout or silently change the
separately pinned generation compiler.

The runtime source includes licensed library test/build resources, not PDZ assets.
Root-level history is newly initialized: unreviewed artwork from upstream Git
history was not imported. This is an independently forkable source repository,
not a literal GitHub fork with shared upstream ancestry. Integration inventories
record reference commits and file differences; they are not claims that features
are already independent cherry-picks.

The v0.9.0-rc1 developer and clean Windows packages were gameplay-tested and
accepted by the maintainer on 19 October 2026 after a fresh committed-source build.
See [release notes and verification scope](RELEASE_NOTES.md). This is a release
candidate, not a claim that every hardware/device/mission combination is verified.
No automatic upstream CI workflows are installed at this repository's root.

## Licences

The game's MIT licence is in PerfectDarkZeroRecomp-v13/LICENSE. The modified SDK
and dependencies retain their own licences/notices in rexglue-sdk-v13 and the
game's third-party notice bundle. The game licence does not relicense the SDK or
all dependencies. Matching source/build/relink information must accompany binary
delivery where required; this repository is not a legal certification.
