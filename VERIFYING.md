# Verifying a source checkout

This is a byte-exact source snapshot. Git text conversion is disabled at the root
and repeated at the end of the included nested attribute files so vendor rules
cannot silently change the reviewed files. Original vendor attributes and notices
are retained; the appended rules are packaging metadata, not vendor code changes.
Normal textual Git diffs remain available. Do not renormalize this delivery without
updating the manifests and repeating checkout verification.

From PowerShell in this repository:

```powershell
./tools/verify-source-delivery.ps1
```

This hashes the game and SDK source files against their supplied manifests. It
does not compile, download game data, run the game or prove gameplay correctness.
Use the game build guide for the separately pinned compiler, legal game inputs
and generated-hook adapters. The runtime SDK is not the generation compiler SDK.

Preparation tested fresh Git-tree checkouts with core.autocrlf both false and true.
A real local transport clone of committed source (without hardlink shortcuts)
also passed game/SDK hashes and clean-tree checks. The maintainer supplied the
GitHub identity used by the source commit. See RELEASE_NOTES.md for the exact
runtime source commit and later gameplay acceptance; these are distinct checks.
Build/test outputs belong outside this source delivery or in ignored directories.

## Optional SDK instruction tests

Prebuilt Linux executables under SDK tools/binutils are deliberately omitted from
this source-only package. The licence and text script are retained. Normal game
builds leave REXGLUE_BUILD_TESTS off; the game algorithm tests do not use these tools.
The optional SDK PPC instruction suite still requires a suitable external PowerPC
toolchain installed at the paths in cmake/ppc_test_pipeline.cmake. Its VMX128 flags
mean an arbitrary stock assembler is not established as compatible. The precise
toolchain source/version and this optional suite remain unverified here. Do not
claim those SDK tests were run or silently enable them for a normal game build.
