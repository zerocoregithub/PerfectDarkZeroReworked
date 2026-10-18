# Perfect Dark Zero Reworked — source

An optional controls, gameplay and presentation enhancement project based on
[PerfectDarkZeroRecomp](https://github.com/nikolaygorb/PerfectDarkZeroRecomp)
and a modified ReXGlue SDK. Original PC recompilation, third-person foundation
and local profile work include upstream contributions; retain their attribution.
Not affiliated with Microsoft or Rare.

**No game assets are included.** Users must supply a legally owned supported
Perfect Dark Zero ISO. A precompiled clean portable package extracts local game
data on first launch; that is not source compilation. Source builders supply
their own supported executable for generation. See [building](BUILDING.md) and
the [player guide](release/README.md) for the distinct workflows.

Supported guest input is the US/NTSC-U TU0 executable, CRC32 `66DC74B6`, Media ID
`41EEAFCB`. Different executable revisions, including USA Rev1, are not supported
simply because their ISO has a similar filename. No game data is downloaded.

## For developers

- [Architecture and ownership](docs/ARCHITECTURE.md)
- [Upstream integration and dependencies](docs/UPSTREAM_INTEGRATION.md)
- [Contribution and comment conventions](CONTRIBUTING.md)
- [Agent instructions](AGENTS.md)
- [Known limitations](KNOWN_ISSUES.md)
- [Third-party notices and corresponding source](THIRD_PARTY_NOTICES.md)

The matching modified SDK is required; an arbitrary upstream SDK or SDL version
does not reconstruct this project. Keep game policy and SDK changes separable
when contributing. The generation compiler is pinned independently from the
runtime SDK; never silently regenerate accepted guest code with a newer compiler.

Presets, input and gameplay helpers have focused tests. Modernized derives from
Classic with explicit overrides; all252 effective values have a frozen reference.
Compilation or these tests alone do not certify gameplay, performance or hardware
compatibility. Experimental and unfinished features remain identified as such.

This source staging is prepared for review, not an already published GitHub fork
or independently cherry-pickable feature series. The integration guide explains
the historical bases and coupled interfaces. No proprietary/private artifacts or
unreviewed artwork should be introduced through imported Git history.

Preserve the project MIT licence and all SDK/dependency notices. Their licences
are not replaced by the project licence; distribute matching source/build and
relink information where required. No legal certification is implied here.
