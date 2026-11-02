# Validation: 0.2 candidate

## Executed

- Linux host tests of double-tap detection: held input, timeout, weapon changes including long guns, actor change, disabled gameplay, empty hand, timer rollover and rearming.
- Linux host tests of action session: ordered sequence, completion, idempotent cleanup, missing resources, failed draw, failed/partial set application, sequence allocation failure, explicit cancellation, actor replacement/deletion, task watchdog and timer rollover. Assertions check restoration of owned ready mode on terminal paths and forbid actor modification on resource failure.
- Windows x64 ASI built with MinGW GCC 14.2, C++17, -Wall -Wextra -Werror. No uploaded executable was run.
- Native recipe verified by static analysis of the user-provided Rampage.asi; menu/callback link and exact native arguments recorded in rampage-recipe.md. All native hashes checked against the RDR native database. Published ABI exports checked against the provided ScriptHookRDR.dll.
- Archive integrity and PE architecture/dependencies inspected.

## Not executed

Full Windows game startup, visible spin, control recovery on the real game, gamepad bindings, mission/mod interactions, and the MSVC build script. Host tests model the control flow; they do not prove native behavior. 0.2 is a test candidate, not a confirmed working release.

## Prior runtime evidence

0.1 produced no visible spin and loss of controls according to the user. The log showed three attempts with cancellations and no normal completion. Its TASK_PLAY_ANIM implementation lacked the action tree and sequence used by Rampage, and passed 0 rather than 1 to SET_ANIM_SET_FOR_ACTOR. That release remains withdrawn. A positive IS_ACTOR_ANIM_PLAYING value did not establish visual success.

## Reference sources

- User-provided Rampage.asi and ScriptHookRDR runtime/SDK.
- User-provided RDR.exe was inspected read-only for native handler signatures.
- https://github.com/TheRouletteBoi/rdr-nativedb-data
- https://github.com/EvilBlunt/RDR-Strings-and-Enums/tree/main/InputStrings

None of the uploaded binaries/assets is redistributed. No fixed Rampage/game address is used by the mod at runtime.
