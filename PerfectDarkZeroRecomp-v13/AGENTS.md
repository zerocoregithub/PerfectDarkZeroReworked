# Maintainer / agent starting point

Read `docs/ARCHITECTURE.md`, `docs/UPSTREAM_INTEGRATION.md` and `CONTRIBUTING.md`.
In the private development workspace also read DEVELOPMENT_STATUS.md and the
relevant research. Those private notes are not necessarily in a public export.

## Boundaries

- Preserve accepted behaviour before restructuring. Propose large refactors,
  feature removals and changes to the sibling SDK separately from fixes.
- Build targets are explicit in CMakeLists.txt. Do not glob every src/*.cpp:
  `game_patches.cpp` competes with the active app/mouse path; old diagnostic
  sources are intentionally unlinked. A file being present is not an active feature.
- Do not edit generated/default as the durable fix. Update the adapter, keep its
  register/ordering contract and run the applicable hook and generation fixtures.
- Generator and runtime SDK are separate pinned inputs. Preserve the legacy
  generation contract in codegen/README.md; never silently use a newer compiler.
- Trace/Observe names can be gameplay dependencies. Do not gate live dodge,
  cover, HUD or targeting state behind a diagnostics option.
- Guest addresses are TU0-specific big-endian addresses, not host pointers.
  Bounds checks alone do not establish mapped memory, lifetime or ownership.
- SDK input filters can run on guest and UI polling threads. Preserve locking,
  input provenance, sample watermarks, capture/drain and neutral-restoration rules.

## Verification and publication

- Normal builds remain quiet. Do not enable logging, launch/install a build,
  modify player settings/saves or change Windows devices as part of a code review.
- Run focused assertion-enabled tests first; compilation and structural tests
  do not prove gameplay correctness or performance improvement.
- Keep patches small and explain why a non-obvious guard exists. Document units,
  ownership, thread context, reset/disable paths and evidence limitations.
- Never add ISO/XEX, generated guest code, extracted assets, caches, captures,
  game fonts/artwork, personal configs/saves, credentials or build outputs to Git.
  Use a fresh reviewed per-file export, not `git add .` in this private workspace.
- Historical notes and compatibility keys are not disposable merely because
  ordinary Release omits their diagnostics. Do not revive parked requests.
- Prefer an existing build directory or small isolated tests over duplicating
  entire portable installations. Keep current, rollback and clean release sets.

There is no authorization here to publish, push, create PRs or remove features.
