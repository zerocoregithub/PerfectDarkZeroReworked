# Perfect Dark Zero Reworked v0.9.0-rc1

Initial public release candidate, 19 October 2026. Built on PerfectDarkZeroRecomp
and ReXGlue; upstream authorship, licences and the third-person foundation remain
credited. This is a separately initialized source repository, not a shared-history
GitHub fork. See README.md and the integration guide for contribution boundaries.

## Install the Windows x64 package

Download the release ZIP, extract it into a writable folder and place your own
legally owned supported ISO in ROM. Launch PerfectDarkZeroReworked-v0.9.0-rc1.exe.
First launch extracts local game data; it does not download a game or compile the
source. The supported US/NTSC-U executable has CRC 66DC74B6; USA Rev1 is not
supported. Settings and saves stay inside the portable folder's Data directory.

No ISO, extracted game assets, generated guest source, private saves/settings,
captures or developer workspace is included in the published source or clean ZIP.
After extraction, your local installation contains private game data: do not
redistribute it. The release package is the original audited, asset-free ZIP.

## Verification and exact source

- Runtime source: c4bd886fd41bbfd3b3241a205497454289b4bf77, including the matching
  modified SDK. Subsequent root release-documentation edits change no runtime code.
- Independently pinned generation compiler:
  f5337cdc947ff6d4c4196737e2c807a48f2a1fc2. Do not substitute the runtime SDK's
  newer compiler; follow the supplied build guide and generated-hook adapters.
- Fresh generation/adapters and full Release build passed. The generated CMake
  support file adds one reviewed empty line between commands; this derived
  formatting difference is not a runtime-source change.
- 28 assertion-enabled algorithm suites passed, including 252 effective preset
  values, plus quiet-runtime, silent-audio pacing/reset and concurrent audio-queue
  harnesses. These checks are not game/device tests.
- The maintainer tested and accepted both the developer and clean packages.
  Individual checklist cases, all ISO variants and every hardware combination
  were not separately demonstrated. No measured performance improvement is claimed.
- The clean ZIP audit verified all 74 allowlisted payload files and hashes;
  the corresponding developer/clean core binaries match. Logging is disabled.

Original clean ZIP: PDZ-Reworked-Clean-v0.9.0-rc1-Reviewed.zip

SHA256: 2BD8F81858B0B9F5F9171CED2D7F55706581FBFD1FC7B2012D176C9F430F0FDB

Complete modified SDK/dependency sources, notices and runtime rebuild/replacement
instructions accompany this source repository. Library targets can be rebuilt
without proprietary game input. Preserve the separate component licences; the
project MIT licence does not relicense the SDK or its dependencies. The bundled
third-party inventory is not a legal certification.

## Important limitations

Windows x64/DX12 is the primary tested path. Native Linux, Vulkan and full Proton/
Steam Deck feature parity are not qualified. Gyro requires an exposed motion
sensor; virtual Xbox input does not supply one. Steam desktop mappings can still
duplicate input when launching outside Steam while Steam is running.

Third person extends the upstream camera, with shoulder transitions, clearance/
impact indication and scoped-eye alignment. Upper-body pose does not follow the
free-aim cursor. GameCube shading and stable cover camera remain experimental;
weapon-attachment hiding, character transparency, pickup outlines and hologram
distance fixes are not completed features. HDR is SDR-derived scRGB output.
The exact rapid-grenade/jungle-projectile crash triggers remain unconfirmed.

Read PerfectDarkZeroRecomp-v13/KNOWN_ISSUES.md for the detailed limitations. Its
packaging-time acceptance warnings are superseded by the dated user acceptance
above, not by a blanket claim of crash-free or universally compatible gameplay.
