# Source provenance and staging status

This source-only repository was prepared on 18 October 2026 from the reviewed
Reworked working source and the verified modified SDK source delivery. It does
not import an old developer repository, portable installation or upstream artwork
history. No PDZ ISO, extracted assets, generated guest program, user saves/settings
or binary runtime is supplied.

Historical game base:99ddda11c40d5d605f9b669310f0cb396e63194e (upstream third-person
mod). Locally reviewed upstream reference:4cda209a6e13593c4caca0987b5aa44221115f3d.
Historical runtime SDK base:bd833a2ab69d305f99ddf57418fc38ffec8af93d. These are
reference/provenance identifiers, not assertions that this tree is pristine at
those commits or that they are the latest remote revisions.

The independent generation compiler remains pinned to
f5337cdc947ff6d4c4196737e2c807a48f2a1fc2; see the game build guide. Its source was
rebuilt and generation/source-archive build verified in the earlier release pass.
Do not replace the runtime SDK with that compiler-era SDK.

The game SOURCE-MANIFEST.csv records current exported files and hashes. The SDK
SOURCE_ARCHIVE.json records its complete modified source, including dependency
contents; upstream pins alone are insufficient for local vendor modifications.
The SDK snapshot has the separately reviewed input-contract comment changes and
appended byte-preserving Git packaging attributes relative to the earlier verified
SDK source delivery. Vendor code and original attribute rules/notices are retained.
Prebuilt Linux binutils executables are omitted only from the public snapshot;
optional SDK instruction tests require an external compatible toolchain. Their
source/version is not established here. It has no new runtime policy. See
VERIFYING.md for checkout verification and test limitations.

The game preset cleanup removes an obsolete stored Modernized column and an
unreachable branch. All 252 effective values were compared against an executable
compiled from the pre-cleanup header, and a frozen reference test was added.
Installed binaries, settings/saves and previous release bundles were not replaced.

The source map/SDK delta inventory separates game, generic SDK interfaces,
game-specific SDK policy and vendored dependency source. These inventories aid
review; they are not a complete tested set of independently applicable patches.

The maintainer supplied the Git identity zerocoregithub and their GitHub noreply
email. Reviewed runtime source was committed as
c4bd886fd41bbfd3b3241a205497454289b4bf77. A fresh build from that commit and its
matched SDK passed the engineering checks described in RELEASE_NOTES.md; both
developer and clean packages subsequently received general user gameplay
acceptance on 19 October 2026. Publication was explicitly authorized afterward.
The later release-status documentation does not modify game/SDK source or the
tested binary package. Per-scenario and hardware limitations remain documented.
