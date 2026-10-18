# Reworked code-generation contract

Do not distribute generated guest C++ or the game XEX. Users provide their own
supported game input. `codegen.partition.json` is function-address-to-file metadata,
not executable code or a substitute for the game. Keep it as a versioned generation
input: existing adapters use those file assignments. Its accepted SHA256 is
99DECE2FEA09028DFB7D451B564B5EAAAAE04CFA71AAFFE215349E1F2C3DE87B.

Current preservation route uses the legacy generator source at ReXGlue commit
f5337cdc947ff6d4c4196737e2c807a48f2a1fc2. Do not downgrade the modified runtime
SDK/controller implementation to that revision. Generator and runtime are separate
build inputs. The compiler has now been rebuilt from that pinned source and its
recursive dependencies. Its regenerated 200 C++ units match the reviewed legacy
generation after the same adapters. No historical compiler binary is required.

Upstream commit7f7c92ef62a13b6aea66ab3f55a6803c4ae1f2f6 changes atomic CAS,
CR0.SO and MSR fencing. Current runtime SDK includes that newer generator source;
using its generator silently changes accepted guest emission. Seed the partition,
generate in isolation with the legacy compiler, apply adapters, and run
`tools/test-codegen-contract.ps1`. That representative check is a guard, not a
complete semantic proof. Normal wrappers preserve repaired generated output;
SDK auto-regeneration requires explicit opt-in and is not the release route.

Isolated legacy generation plus current adapters matches391 of401 generated
C++/header files after line-ending normalization. Reviewed remaining differences
are include ordering/unused includes, equivalent P9P scopes/braces, and the native
825A8080 forwarding function now emitted from an existing boundary declaration.
The host fallback already checks for an existing function before registration.
No accepted gameplay code was changed to produce this comparison.

An isolated full source-build/link and quiet-runtime/silent-audio/queue harnesses
have passed, including a complete delivered-archive rebuild. Gameplay qualification
remains a separate gate. Required generator/dependency source and notices must accompany
the eventual source delivery; these notes are not upload approval.
