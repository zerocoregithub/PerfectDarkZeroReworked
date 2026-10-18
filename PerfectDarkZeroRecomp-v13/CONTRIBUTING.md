# Contributing to Reworked

Reworked extends PerfectDarkZeroRecomp and a modified ReXGlue SDK. Third person
and local profiles include upstream work: do not present the entire PC port or
camera foundation as newly authored here. Preserve copyright and licence notices.

Start with [architecture](docs/ARCHITECTURE.md) and
[integration boundaries](docs/UPSTREAM_INTEGRATION.md). A fork should be able to
take one feature without adopting every preset or experimental renderer choice.
That is the integration goal, not a claim that all features are independent today.

## A useful change proposal

Describe the user-visible problem, reproduction, supported executable revision,
feature/preset and controller/backend involved. Identify the smallest owner:
pure algorithm, game adapter, overlay/settings, runtime API or renderer. State
what must not change, evidence used, tests run and remaining gameplay checks.
Do not attach proprietary game files, captures containing game data or private saves.

Prefer pure helpers with small tests, then explicit native hook call sites.
Keep generic SDK facilities separate from PDZ policy. Do not fix a guest-state
problem by guessing a field or changing shared camera/muzzle/input transforms.

## Comments and structure

Comments should explain intent and constraints, not narrate obvious assignments:

- Guest hook: verified function/call site, register arguments, phase/order,
  allowed owner/type and native fallback on a failed guard.
- State: producer, consumer, units, owning thread/lock and reset/freshness rules.
- Rendering: actual shader/pass identity, what is isolated and what is unverified.
- Compatibility: why a legacy name/adapter remains and the test guarding it.

Keep feature guides in docs rather than adding long transcripts to functions.
Never mark a heuristic as proven or imply a render-only value moves the muzzle.
Do not introduce large formatting-only diffs into reverse-engineered code.

## Tests

On Windows with Clang in the developer shell, run the small pure suite with an
explicit matched SDK and a new, small output directory:

```powershell
./tools/test-release-pure.ps1 -SDKSource ../rexglue-sdk-v13 -OutputDirectory ./out/review-tests
```

`tools/test-gyro-routing.ps1`, `test-locktopus-hook.ps1` and
`test-third-person-release-cleanup.ps1` are structural checks of this workspace's
source/generated call sites. They are not hardware or gameplay tests. After
changing generation adapters, run `test-publication-cleanup.ps1` in a disposable
fixture and verify the code-generation contract. Do not regenerate the live
accepted build merely to run a review.

Build prerequisites and source delivery are in release/BUILDING.md (BUILDING.md
at the root of a public delivery). Guest inputs must be supplied legally by the
developer. Do not commit outputs of generation or the test executables.

Review whether fixtures still match before removing an old test. Add a regression
that exercises the effective policy, not only a historical table column.

## Upstream-friendly changes

Use focused commits with rationale, dependencies and tests. Do not bundle shader
experiments with gyro, audio startup, launcher changes or gameplay policy. An SDK
interface change needs a corresponding host update and a matched ABI build;
replacing DLLs in an unrelated upstream build is not an integration method.

Do not delete compatibility options or unlinked upstream reference files without
an explicit migration/removal decision. Source review, gameplay acceptance and
distribution approval are separate gates.
