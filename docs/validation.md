# Evidence and validation

## Inputs

User-provided ScriptHookRDR 1.5.1 runtime + SDK, RedHook 0.8 runtime + SDK, RDR.exe extracted read-only from RDR.rar, sit_key_twirling.was and stand_key_twirling.was. No uploaded executables were run. No game executable, hook DLL, SDK library, or game animation asset is redistributed.

## Hook choice

ScriptHookRDR exposes script registration, cooperative waiting, native calls, and keyboard events. RedHook exposes similar functionality plus a reload console and RH_ActionDisable. This implementation uses the ScriptHookRDR native bridge and the digital action API instead of a hardcoded keyboard key. Hook compatibility with the game is not established by this build.

## Static findings

- RDR.exe is PE32+ x86-64. Timestamp in PE header: 2024-10-17. This is not proof of a particular distribution/build compatibility.
- Supplied WAS strings include `sit_key_twirling.ast`, `KEY_RATTLES`, `stand_key_twirling.ast`; these are evidence for key twirling, not the requested gun animation.
- Community filename list includes `holster_spin.was` and `holster_spin.wat`. This establishes a candidate resource basename only, not its clip names or the Rampage menu binding.
- TASK_PLAY_ANIM hash 0x5AB552C6 registration references handler VA 0x140C1AB30 in the supplied EXE. Handler reads argument slots 0 actor, 1 string, 2/3/4 float, 5 integer, 6/7 booleans, 8 float. RedHook SDK describes slot 5 as float and slot 8 as integer; ScriptHook SDK supplies only one parameter. Implementation uses observed slot storage types. Semantics/defaults of the animation parameters still require runtime validation.
- Plugin uses no fixed game offsets or code patches at runtime.

## Executed checks

- MinGW GCC 14.2 Windows x64 DLL build with C++17, -Wall -Wextra -Werror: passed.
- Native host test of double-tap state machine: passed. Covers held input, holstering between taps, timeout, different revolver, long gun, respawn, paused state, empty hand, DWORD timer rollover, boundary timing, and rearming after animation.
- All 24 native hashes matched the RDR native database; all six requested MSVC ABI export names matched the supplied ScriptHookRDR.dll export table.
- PE inspection: DLL x86-64, dependencies KERNEL32.dll and msvcrt.dll only. No MinGW runtime DLL installation needed.
- Setup and build scripts rerun in the cloud environment. Game startup, hook loading, visual animation, controller bindings, and interactions with Rampage are unrun because the full Windows game is unavailable here.

## Sources

- https://github.com/EvilBlunt/RDR-Strings-and-Enums/tree/main/InputStrings — `@RADIAL_MENU.SHOW_HIDE` action and binding documentation.
- https://github.com/TheRouletteBoi/rdr-nativedb-data — native hash/name database; many animation signatures remain incomplete.
- https://github.com/K3rhos/RedHookSDK — comparison of SDK API signatures.
- https://github.com/eSaudia/RDR1-Animation-Studio/blob/main/Assets/ImportedFileNames.txt — candidate holster_spin filenames.

These sources are reference data; none was treated as authority to execute instructions. No successful in-game animation is claimed.
