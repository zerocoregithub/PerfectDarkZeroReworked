# Public source scope

The export uses a reviewed per-file SHA256 manifest, not a recursive copy of a
private development workspace. Review the actual staged tree before any upload.

Included: active game sources and pure helpers, explicit build/adapter tooling,
current tests and reference data, contribution/architecture guides and licences.
Upstream game_patches.cpp/.h remain unlinked reference sources, not a second
enabled mouse implementation. CMake's explicit source list is authoritative.

Withheld from this publication staging (retained privately):

- ISO/XEX, generated guest C++, extracted assets and game fonts/shaders/caches.
- Personal settings/saves, captures, diagnostic dumps and private research.
- Unreviewed icon artwork and its unused Windows resource script.
- Retired P9P tracing source; its adapter migration still recognizes old includes.
- Parked A/B-comparison and pickup-outline helpers/tests, not needed by the game.

The compiled experimental P9P model path is not removed here: it remains a known
unfinished feature. This scope decision does not revive it or claim it works.
Historical work remains intact in the private source tree.

Generic SDK source includes licensed dependency build/test resources. Those are
not PDZ game assets; exact dependency contents and notices must remain matched.
Modified vendor files cannot be reconstructed by merely selecting a pristine
upstream submodule commit. The supplied source-archive manifest verifies that
delivery; it is an integrity check, not a signature or legal certification.

Public staging appends byte-preserving Git packaging rules without changing vendor
code. Bundled Linux binutils executables are omitted; the optional SDK instruction
tests require an external compatible toolchain. This does not affect the normal
game build or remove test source. See the umbrella VERIFYING.md for limitations.

Root README in the public tree is SOURCE_README.md from this release directory.
The player guide remains under release/README.md. Older release ZIPs and source
manifests are immutable historical evidence and are not silently overwritten.
