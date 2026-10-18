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

This staging has not been uploaded or gameplay-qualified as a new release. Keep
source checks, gameplay acceptance and distribution approval distinct. No automatic
upstream CI workflows are installed at this repository's root.

## Licences

The game's MIT licence is in PerfectDarkZeroRecomp-v13/LICENSE. The modified SDK
and dependencies retain their own licences/notices in rexglue-sdk-v13 and the
game's third-party notice bundle. The game licence does not relicense the SDK or
all dependencies. Matching source/build/relink information must accompany binary
delivery where required; this repository is not a legal certification.
