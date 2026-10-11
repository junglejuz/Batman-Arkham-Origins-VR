# TEST PLAN OP1B — build, forwarding checks, then one passive game session

Looked up: `RE_NOTES.md` OP1 and static/Ghidra receipts; City `scene.cpp` forwarding
and `d3d11_hook.cpp` real-context discovery; workspace essentials §§1/2/3/10;
framework guide 13 discovery/verification; playbook A4.1–A4.4 guards, trampoline
room, guarded reads and self-call exclusion, RE-01/RE-02 and agent workflow
contract gates, checkout `b955f2244f7c5d5f3b3e5f403b1b6721a3362423`.

Before any build: use an isolated x86 probe and XInput proxy; preserve source
EXE/configs. Engine entry probes save/restore integer registers, flags and
x87/SSE state, then jump to the original trampoline with the original stack.
This avoids assigning unproved C++ argument types to the 112-byte constructor.
The observer reads only the statically established number of stack words.
No camera, lens, simulation, HUD, eye replay, graphics-setting or XR writes.

## Offline checks (no game launch)

1. Build PE32 proxy, probe and synthetic forwarding host using the installed
   VS Win32 toolchain and a private licensed MinHook copy.
2. Hook synthetic x86 functions with 0, 1, 5 and 28 stack words: verify every
   word, ECX/EDX, result, stack balance and exactly one original call. Check
   preserved SIMD input across an observer that deliberately clobbers it.
3. Load proxy in a separate x86 host: compare standard XInput exports and
   ordinal 100 against the system DLL, and verify flat launch loads no probe.
4. In a foreign host with the probe flag, verify identity refusal before any
   graphics device or engine hook is created. Check PE architecture/exports,
   executable hash, manifest and launcher/install/remove dry checks.

## One game session (JJ drives; tools send commands and read receipts)

The game remains on the desktop for this passive census. No OpenXR session
exists, so a fake/real headset run is unnecessary at this stage. Check other
Steam games before launching; Arkham Knight was running at implementation start.

| # | Live command / stage | Proof and branch |
|---|---|---|
| 0 | `status`, `mark title`, `probe off` | Load/hash/command acknowledgements and original flat control; D3D11 Present must actually fire. If absent, retain module census; do not claim D3D11 from imports |
| 1 | `hooks engine`, `probe on`, `mark gameplay` | Signature guards and hit counts for root, begin/end, CalcSceneView and constructor, separated by caller/thread. Zero candidate hits → bounded raw viewport virtual-target snapshots; do not replay |
| 2 | `capture state`; still, camera turn | Bounded raw stack/matrix/rectangle/view-state candidates; finite matrices and changing camera values. Invalid reads reported, no write or sentinel camera |
| 3 | `hooks context`, `capture state`, `modules` | Game-device immediate/deferred method targets, draw/dispatch/command-list/tessellation counters and bounded colour/depth/viewport descriptors. Synthetic deferred context discovers its table; log actual types of contexts that hit |
| 4 | `mark pause/map/detective/zoom/menu`; visit and return | Caller/counter/view-state changes per user-labelled mode. Marks are annotations, not automatic mode classification |
| 5 | `mark snow-paused`, `mark snow-running`, `capture state` | Available draw/dispatch evidence near snow/reflections. Scene-capture update and simulation-tick consumers are **deferred** until separately identified; this cannot prove once-per-tick effects |
| 6 | `probe off`, `status`; load/menu/exit | Measurement counters stop while original forwarding/Present continues. Retain resize/device-turnover events. No claims of engine setting precedence or executing AO/DoF solely from module names |

Startup creates only the Present/Resize census; engine/context hooks require
explicit live commands and are batch-enabled. `capture state` is bounded;
there is no per-draw file I/O. A `sequence baseline` command should run the
off/on/camera/context/off measurements on a timer at a stationary checkpoint
and restore the previous measurement setting. UI navigation remains JJ's.

Results must distinguish SOURCE, build/offline-host, LIVE game and HEADSET.
Offline forwarding success does not validate retail-engine ownership/re-entry.
The first native two-eye experiment needs a separate side-effect/endurance plan.
