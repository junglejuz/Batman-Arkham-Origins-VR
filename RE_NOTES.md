# Batman Origins — target evidence

Recorded 2026-10-11 (Australia/Sydney), after JJ confirmed installation on E:.
Status: **STATIC inspection only. No game launch or game-file edits in this task.**

Looked up: `ORIGINS_PLAN.md` §§2, 4–7; City `RE_NOTES.md` render-root/ABI discovery and `FIX_CHANGES.md` config encoding; workspace essentials §§2, 3, 7, 9–11. Playbook `reverse-engineered-route.md#re-01`, `AGENT_RE_WORKFLOW.md` contract/evidence rules and `17-teardown-fc2vr-native-stereo.md#side-effect-gate`, checkout `b955f2244f7c5d5f3b3e5f403b1b6721a3362423`. These justify fingerprinting before adopting the donor route; they establish no Origins runtime behavior.

## Installed target

| Field | Observed value | Grade |
|---|---|---|
| Steam manifest | `E:\Games\Steam\steamapps\appmanifest_209000.acf` | STATIC |
| App / build | 209000 / 243683; manifest StateFlags=4 | STATIC |
| Install root | `E:\Games\Steam\steamapps\common\Batman Arkham Origins` | STATIC |
| Intended executable | `SinglePlayer\Binaries\Win32\BatmanOrigins.exe` | STATIC |
| Separate online executable | `Online\Binaries\Win32\BatmanOriginsOnline.exe`; excluded from this mod | STATIC |
| Architecture | PE machine 0x014c (i386), optional header 0x010b (PE32) | STATIC |
| Size / version | 24,327,496 bytes; file/product version `1, 0, 0, 0` | STATIC |
| SHA-256 | `1c373ed6edd3f5d42b097fcb2fc592811671b6f7d54b764b043b01e47871e5c2` | STATIC |
| Preferred image base | 0x00400000; use actual module base plus RVA at runtime | STATIC |
| Entry RVA | 0x02eeb2ee | STATIC |
| Large-address-aware | Already set; no executable patch required for this flag | STATIC |
| Engine-family leads | Executable contains UnrealEngine3 paths and UE viewport/render strings | STATIC strings; exact engine structures unproven |

Initial inspection method: read PE headers, section table and import descriptors directly with Python `struct`; hash the installed bytes with SHA-256; read shipped INIs and check generated-config presence. Subsequent read-only Capstone disassembly is recorded below; no unpacking has been performed.

## Loader and protection leads

Imports include `XINPUT1_3.dll` and `DINPUT8.dll`, so City's XInput proxy is an appropriate **candidate**, with DInput available as another candidate. Neither module name currently exists as a local proxy beside the single-player executable; `dxgi.dll` and `d3d11.dll` are also absent there. Confirm actual import use, export forwarding and load timing in the target before deployment.

PE sections: `.text`, `PSFD00`, `.rdata`, `.data`, `.tls`, `.rsrc`, `.reloc`, `.bind`. The entry point is in `.bind`; its raw-byte entropy is about 7.998 bits/byte. `.text` is about 6.618, unlike City's fully encrypted `.text` observation. A Steam wrapper/protection path is a hypothesis, not an identified version. Do not assert that Origins needs the same unpacker, wait condition or hook sequence as City. Confirm in-memory code readiness before patching any candidate.

## Renderer and special-feature leads

Imports include `d3d9.dll`, `d3dx9_43.dll`, `d3dx11_43.dll` and `D3DCOMPILER_43.dll`. Shipped `SinglePlayer\Engine\Config\BaseEngine.ini:854` says `AllowD3D11=True`; launcher profiles also enable D3D11. **Active D3D11 device/context still requires LIVE observation.** Static imports alone cannot settle the renderer.

NVIDIA imports: `GFSDK_SSAO.win32.dll`, `GFSDK_NVDOF_LIB.win32.dll`, `GFSDK_ShadowLib.win32.dll`, `GFSDK_PSM.win32.dll`, `GFSDK_GSA.win32.dll`. PhysX/APEX imports include `PhysXExtensions.dll` and `ApexFrameworkCHECKED_x86.dll`; shipped supporting files include turbulence, particles, clothing and GPU PhysX libraries.

Transfer hypotheses for the first renderer census:

- Vendor shadow contexts may retain first-eye data: identify the actual feature/pass and compare per-eye outputs before disabling or replacing it.
- NVDoF and the engine post chain may bundle grading/exposure/AA: keep the block until its pass dependencies are measured.
- PhysX/APEX snow or particles may advance during rendering: test pause/run and once-per-tick counters, then render both eyes from shared simulation.
- GSA may contribute a second graphics-settings store like Knight: determine the actual read/write path instead of assuming the same XML filename.

These are INFERENCE, not found stereo defects.

## Configuration baseline

Observed installed files include `SinglePlayer\BMGame\Config\DefaultEngine.ini`, `UserEngine.ini`, `Launcher.ini`, and `SinglePlayer\Engine\Config\BaseEngine.ini`. DefaultEngine/UserEngine/Launcher currently have no BOM; preserve exact encoding/line endings if any future edit is authorized. Generated files may use a different encoding.

At the initial inspection, no `D:\Documents\WB Games\Batman Arkham Origins` user-config directory was present, Steam's manifest had LastPlayed=0, and no `BatmanOrigins` process was observed. This is historical: the subsequent DX11 Enhanced baseline below locates the generated engine config and GSA store after JJ used the graphics menu.

Relevant shipped values, **not effective runtime settings**:

| File / line | Value | Implication to test |
|---|---|---|
| `BaseEngine.ini:849` | `OneFrameThreadLag=True` | Queue/pose/eye ownership needs render-thread evidence |
| `BaseEngine.ini:850` | `UseVsync=False` | Verify actual Present behavior |
| `BaseEngine.ini:854` | `AllowD3D11=True` | D3D11 stock-path candidate |
| `BaseEngine.ini:1161` | `AspectRatioAxisConstraint=AspectRatio_MaintainYFOV` | Differs from City's recorded MaintainXFOV; identify effective player camera policy |
| `BaseEngine.ini:771,774` | MotionBlur/DepthOfField True | Other sections set them False; do not assume the active bucket |
| `DefaultEngine.ini:1187,1189` | `bSmoothFrameRate=TRUE`, `MaxSmoothedFrameRate=-1` | Stock limiter semantics differ from City's old 62 cap; measure actual cadence |
| `BaseEngine.ini:489,491` | another section: smoothing False, maximum 120 | Section ownership matters; do not globally replace matching keys |
| `Launcher.ini:105–107` | D3D11 True, 1920×1080 in one profile | Launcher profile is not proof of the live eye/world render size |

## JJ's DX11 Enhanced baseline — 2026-10-11

JJ reports selecting Direct X Enhanced for several graphics options, including geometry and ambient occlusion. **Subsequent correction: DoF OFF and motion blur OFF. This is the intended testing baseline.** Saved-file inspection found the following **STATIC** values in `D:\Documents\WB Games\Batman Arkham Origins\GFXSettings.BatmanArkhamOrigins.xml`; its DoF entry conflicts with JJ's report and must not be presented as his current effective choice:

| Saved option | Value |
|---|---|
| GeometryDetail | DX11 Enhanced |
| AmbientOcclusion | DX11 Enhanced |
| DynamicShadows | DX11 Enhanced |
| DepthOfField | DX11 Enhanced |
| AntiAliasing | TXAA High |
| HardwarePhysX | High |
| ResolutionX / ResolutionY | 2560 / 1440 |
| Fullscreen | true |
| VSync / MotionBlur | false / false |
| Distortion / LensFlares / LightShafts / Reflections | true / true / true / true |

After JJ's correction, a fresh read still found GSA `DepthOfField=DX11 Enhanced` and engine `[SystemSettings] DepthOfField=True`; motion blur is false in both. Saved DoF state versus the selected/effective setting is unresolved (save timing or settings precedence are possible explanations, not established causes). No config edits were made to resolve this by assumption. The next baseline check must inspect the actual option consumer/post passes and the files after a normal game settings save/exit. JJ's reported DoF-off baseline remains authoritative for planning; it is not yet independently LIVE-verified.

The generated engine config is **inside the installation**: `SinglePlayer\BMGame\Config\BmEngine.ini`. It is UTF-16 LE with BOM. The GSA XML declares UTF-16, contains little-endian UTF-16 text and has **no BOM**. Preserve these different encodings exactly.

`BmEngine.ini` contains `AllowD3D11=True`, motion blur off, depth of field and ambient occlusion on, and a 2560×1440 resolution. Multiple section/profile values coexist. Its observed fullscreen value differs from GSA's saved true value, so file contents still do not prove effective settings or which store wins. The GSA store is now a confirmed file, rather than merely an imported-library lead; runtime read/write precedence remains open.

Saved byte-for-byte baseline copies under [baselines/stock-dx11-enhanced-20261011](baselines/stock-dx11-enhanced-20261011), with source paths, sizes, SHA-256 and the saved option list in [manifest.json](baselines/stock-dx11-enhanced-20261011/manifest.json). Both copies were verified against source hashes. No game configuration was edited. Future installer/profile work must preserve both settings stores and section ownership.

Baseline policy: start render probing with **DoF OFF and motion blur OFF**, plus JJ's enhanced geometry/AO/shadow options. Test geometry/tessellation, enhanced AO, enhanced shadows, TXAA and high PhysX as separate dimensions, recording actual executing passes and per-eye cost/correctness. Do not infer stereo compatibility from the DX11 Enhanced label. Native tests need per-eye temporal history for TXAA; DLSS may replace only a verified AA boundary. Audit the DoF-off post chain for bundled grading/exposure/mode dependencies; the City/Knight lesson is a diagnostic lead, not a reason to override JJ's choice. Any candidate VR defaults follow measured comparisons and headset acceptance.

First test-plan additions: log effective graphics choices at the actual consumers; identify ShadowLib/AO/NVDoF/TXAA contexts and resources; inspect tessellation stages and snow/particle simulation; compare normal/enhanced options where live switching is verified safe. Restart-dependent options are grouped into the minimum separate launches with identical checkpoints. These remain planned tests, not completed LIVE evidence.

## Decisions and remaining P0 proof

1. Confirmed x86 means the City-style x86 mod/OpenXR plus x64 DLSS helper is the planned route; preserve separate toolchains and protocol layout.
2. Use only the SinglePlayer tree. City native-root architecture remains the first rendering candidate, not a proven seam.
3. Stock baseline remains pending: JJ launches/navigates once; record actual device/API, generated config/profile locations, initial settings, a reusable gameplay checkpoint, load/mode behavior and cadence. Batch that work under a numbered plan before launching.
4. Then add passive Origins caller/view probes with validated ABI, signatures and module guards. Origins command boundaries and view candidates now have STATIC evidence below; live ownership, complete camera layouts and shader identities remain unproven.

P0 is **partially complete (STATIC fingerprint)**, not cleared. LIVE stock-renderer and gameplay-baseline evidence are still required.

## Render/view scouting — 2026-10-11

Looked up: framework guide `13-reading-the-engine-object-model.md`, discovery ladder;
playbook `reverse-engineered-route.md` RE-01/RE-02 (tool identity before trusting
results), shared agent workflow contract/evidence gates; City `re/ac.py`,
`scene.cpp` and `d3d11_hook.cpp` (native-root live caller census and game-context
ownership). Playbook checkout `b955f2244f7c5d5f3b3e5f403b1b6721a3362423`.

**STATIC, installed retail bytes:** [re/origins.py](re/origins.py) rejects other
executable hashes before mapping/disassembly. Its reproducible output is
[static-command-map.json](re/evidence/static-command-map.json). Absolute-pointer
matches are byte-search leads; they are not instruction-aware Ghidra xrefs.
No nearest-E8-target heuristic is treated as a function boundary.

| Seam | Preferred-image VA / RVA | Observed contract / boundary |
|---|---|---|
| BeginDrawingCommand execute | `00665560` / `00265560` | `ECX=self`; reads viewport at `self+4`, calls its vtable byte slot `+0x60`, returns EAX=8, `ret` with no stack cleanup |
| Begin descriptor A / B | `00665570`, `006656f0` / `00265570`, `002656f0` | Returns wide BeginDrawingCommand string pointer; separate descriptor tables share the execute function |
| EndDrawingCommand execute A | `00665590` / `00265590` | Reads viewport at `self+4`, calls vtable byte slot `+0x64` with two zero arguments, returns EAX=8, `ret` |
| EndDrawingCommand execute B | `00665710` / `00265710` | Reads flags at `self+8`, calls `00665610(viewport, flags)`, returns EAX=12, `ret` |
| End dispatch | `00665610` / `00265610` | Two stack arguments, caller cleanup; decodes flags, dispatches viewport `+0x64` and updates timing globals |
| Begin A / End A command tables | `01492e98`, `01492ea8` / `01092e98`, `01092ea8` | Three slots: destructor, execute, descriptor; writes in `0067de30` region (special render path candidate) |
| Begin B / End B command tables | `01492ec8`, `01492ed8` / `01092ec8`, `01092ed8` | Same three-slot layout; writes within candidate root `0067ed90` |
| Candidate viewport draw root | `0067ed90` / `0027ed90` | Padding-delimited prologue, ECX copied to EBP, return at `0067f30c` is `ret 4`; enqueue begin → create canvas → dispatch client draw via `viewport+0x20`, vtable `+8` → enqueue end |
| Candidate local-player CalcSceneView | `007d40f0` / `003d40f0` | Called at `0067eea5` with five stack arguments and local-player ECX; aligned prologue, return `ret 0x14`; produces view-sized allocation/constructor path |
| Candidate scene-view constructor | `009cfdc0` / `005cfdc0` | Called from candidate CalcSceneView at `007d4ea7` after preparing viewport rectangle and matrices; ECX=allocated view; return at `009d162f` is `ret 0x70` (112 stack bytes); argument/field semantics still to reconstruct |
| View helper, identity unresolved | `007d5080` / `003d5080` | Calls same CalcSceneView, return `ret 0xc`; **not identified as UGameViewportClient::Draw** |

City's begin executor used viewport slot `+0x64`; Origins uses `+0x60` for
beginning and `+0x64` for ending. This is direct evidence against copying donor
vtable indices. The root's actual gameplay/movies/special-mode callers remain
to count. Its early branch also builds views outside the normal drawing path;
do not identify gameplay from this address alone.

The candidate scene-view constructor cleans **112 stack bytes**, whereas City's
wrapper forwards 108. Origins therefore needs its own reconstructed contract;
copying City's 27-DWORD forwarding wrapper would corrupt the stack. Stack-word
count does not establish the number/types of C++ arguments (some may be structs).

[static-view-candidates.json](re/evidence/static-view-candidates.json) preserves
linear disassembly for candidate root, CalcSceneView and the unresolved helper.
Linear decoding, stack-cleanup sizes and similar argument preparation are not
proof of complete ABI types or safe render re-entry. No hook or camera write has
been made. Class native-table names used by City's finder (`...exec...`) were
absent in the scanned encodings; route through command strings/call graph instead.

### Ghidra tool preflight

The supplied MCP answered, but its program had `.text` at `140001000` and an
entry at `1464caee4`: a different, x64 program. Local window/process discovery
identified the port-8080 listener as the **Assassin's Creed Unity** CodeBrowser.
Origins and City CodeBrowsers were also open, but neither had its own listener
at the initial check. No rename, prototype or comment operation was sent to the
wrong program. JJ was asked to configure the Origins bridge on a separate port
(8082) and reload the plugin; confirmation/preflight remains pending.

**Follow-up:** after JJ adjusted Ghidra, the supplied MCP on 8080 returned
Origins' x86 section layout and entry `032eb2ee`. The imported image layout now
matches the target. JJ reports analysis is still running. The bridge currently
finds no defined function at `00665570` or `0067ed90`, no BeginDrawingCommand
defined strings and no command-table xrefs. That is incomplete-analysis evidence,
not evidence that the code is absent; the file disassembly above remains valid.
Do not interrupt/restart ongoing analysis or treat empty Ghidra results as a
negative gameplay finding. The exact section/descriptor guard currently blocks
the HTTP reader until the descriptor has been defined.

If those seams remain undefined **after analysis completes**, the optional
[OriginsDefineRenderCandidates.java](re/OriginsDefineRenderCandidates.java)
script validates the program's executable hash and every candidate's imported
16-byte prefix before creating missing disassembly/functions. It preserves
existing functions and listing conflicts, sets no prototypes and edits only
Ghidra analysis. It was subsequently compiled and executed in a **separate
headless project**, described below; it has not run in JJ's open project. The supplied
MCP has no script-execution or create-function endpoint; using it requires one
Script Manager invocation in Ghidra. Record any actual execution and created
functions separately. Normal decompiler/xref reads can then proceed through MCP.

### Auto-analysis failure and successful focused Ghidra fallback

JJ reported the automatic analysis finished with an error. The local Ghidra
12.1.2 `application.log` records a Decompiler runner timeout at 03:29:55 AEDT,
then Auto Analysis failure at 03:29:57: `Timed-out waiting for Swing thread lock
in 20 SECONDS`. A recovery snapshot was created at 03:30:19. This identifies
the reported UI/analysis timeout, **not its underlying cause**. Earlier missing
PDB and invalid-PNG messages are not established causes. A subsequent `jcmd
Thread.print -l` showed the UI thread idle; no persistent JVM deadlock was
established. Raw diagnostic excerpt and thread dump are retained in `re/evidence`.

To continue without altering the open project, imported the same retail EXE
into `re/headless/OriginsRender_20261011.gpr`, disabled broad automatic analysis,
and ran the two focused scripts. The source EXE was read only. Ghidra metadata
matched the SHA-256 and x86 language, and all ten imported-memory 16-byte prefix
guards passed before definitions were created. Both scripts compiled and ran;
all **10/10** functions completed decompilation, and the isolated project saved
successfully. Evidence: [ghidra-focused-render.txt](re/evidence/ghidra-focused-render.txt),
[headless log](re/evidence/ghidra-headless.log),
[script log](re/evidence/ghidra-headless-script.log).

This is **STATIC/Ghidra** evidence. The decompiler inferred incomplete/incorrect
types for some unanalysed callees; for example, it omitted a stack argument from
the root prototype despite the instruction-aware `ret 4`, and initially displayed
an incomplete EndDrawing dispatch prototype. The constructor also reports type
propagation not settling. Do not turn these inferred declarations into hook ABI.
Use actual call-site pushes, ECX ownership, stack cleanup and live pass-through
proof together. The 112-byte constructor cleanup is confirmed in both Capstone
and Ghidra disassembly.

New concrete camera leads: constructor copies two 16-word inputs from stack
words 14/15 into `view+0x60`/`view+0xa0`, then builds further matrices. City uses
those argument positions for view/projection; in Origins that semantic assignment
remains a hypothesis until the caller/math and live values are checked. This
supplies a bounded first-probe input contract without assuming all other City
fields match. The extra final constructor argument needs its own interpretation.

Checks completed: static command and view reports reproduced from the guarded
scanner; a foreign EXE rejected by the hash guard; seed script compiled against
the installed Ghidra API, then both scripts executed successfully headlessly.
**No game launch, camera modification, renderer hook, XR or headset test.**
The GUI auto-analysis failure has been bypassed for this investigation; JJ's
original project has not been repaired or rewritten. Automatic Ghidra loader
cache outputs are recorded in [FIX_CHANGES.md](FIX_CHANGES.md).

[re/ghidra_read.py](re/ghidra_read.py) can read that bridge directly using the
same GhidraMCP GET endpoints. It checks Origins section starts and the begin
descriptor anchor before any requested function/xref read. The guard is an
identity cross-check, not a full hash of Ghidra's imported program. Read-only
HTTP access avoids changing the global MCP configuration or another instance.

Ghidra follow-up order: validate function boundaries and calling conventions;
trace root callers/client draw and scene-view constructor; reconstruct view
matrix/projection/rectangle/view-state arguments and inverse/culling writes;
find AllocateViewState and scene-capture refresh; trace graphics-setting
consumers and special post passes. Save each raw receipt, including negative
results, before naming or relying on a seam.

## TEST PLAN OP1 — historical passive proposal (superseded by built VR O1B)

This replaces no later O1 stereo tests; it supplies their missing ownership
evidence first. Commands below are **required probe interfaces, not available
runtime commands yet**. No game launch is requested until this implementation
is built and checked. Preserve JJ's DoF-off/motion-blur-off setup and both
configuration stores. No graphics options need changing for the passive run.

| # | Live command / stage | What it proves | Judgment / prepared branch |
|---|---|---|---|
| 0 | `probe off`, `status`; title/menu | Injection identity and forward-only control | Executable/probe hashes, loaded module base, target guards and command acknowledgements; ordinary flat behavior. Guard mismatch → no engine hooks |
| 1 | `probe on`, `mark gameplay`; enter reproducible checkpoint | Live draw root, client draw, CalcSceneView and begin/end owners | Counts per caller/thread/viewport and Present; registration success alone is not a hit. Zero root hits → log actual command viewport/client virtual targets, no replay |
| 2 | `capture state`; stationary, then camera turn | Camera/view constructor and size ownership | Raw input argument snapshots, finite camera matrices, live colour/depth/viewport sizes and lens conventions. Unexpected values → bounded raw dump, no writes |
| 3 | `mark pause/map/detective/zoom`; each mode and return | Mode-specific draw/camera/state lifetimes | Counter/caller changes and view-state identities; unresolved identity stays unresolved rather than frame-count classified |
| 4 | `mark enhanced`; graphics menu then gameplay | Actual device/API and enhanced-stage execution | Game-device/immediate and deferred context census; tessellation stage/pass counters and vendor module/context evidence. DLL presence does not prove an option executes |
| 5 | `mark paused/run`; pause, unpause and move near snow/wet surface | Follow-up leads for captures and render-time effects | Scene-capture/tick/render counters with trigger receipts, if those hooks are already ABI-validated. Otherwise explicitly defer this row |
| 6 | `probe off`, `status`; load/return/menu exit | Control restoration and lifecycle | Measurement counters stop while original calls continue once; loading/resize/device turnover logged. No stereo, simulation duplication or XR session in this census |

Each stage logs an acknowledgement, active switches, actual hook-hit deltas and
thread IDs. Expensive snapshots are bounded and can be disabled live. Count
and tag device/context identities; no per-draw file I/O. Prepare timeout/missing
hook branches before launching, batch MinHook enables, and keep new riskier
hooks disabled until the baseline has been collected. Any future native replay
needs its own side-effect/endurance plan; passive counts cannot clear that gate.

## Implementation update — actual VR build O1B

JJ explicitly rejected a probe-only deliverable. BUILD_PLAN.md records the
replacement implementation/test scope and completed build/deployment results.
The x86 DLL now contains guarded native viewport replay, the target-specific
28-word constructor adapter, complete located per-eye poses/lenses, render-thread
eye/generation labels, HUD/panel/output transport and AFW/DLSS integrations.
An independent x64 helper is compiled and installed. User gameplay ownership
selection is required on the first run, not inferred from a donor caller.

The allocator at `009d4920` is a new STATIC lead validated from retail bytes:
cdecl/no arguments, 0x350-byte allocation, construction through `009d2180`,
EAX stored by the local-player initialization at `007d554f` into self+0x7c.
Instruction receipt: re/evidence/view-state-allocator.json. It is not yet a
live state-lifecycle or special-mode proof. Donor method: City scene.cpp private
right-eye state, workspace VR_ESSENTIALS §2; playbook STR-009 and chapter 17
side-effect gate, commit b955f2244f7c5d5f3b3e5f403b1b6721a3362423.

Build/offline and installed file/config receipts are in re/evidence/build-checks.json,
build-static-guards.json and installed-build.json. The build checks include the
actual constructor detour's 28-word forwarding and asymmetric lens corner/depth
math. They do not establish the Origins gameplay route, native render safety or
headset correctness. No game/runtime/headset run has been performed.

## O1C update ? first LIVE route, HUD boundary and movie stop

O1B-first-live.log records a real VDXR/Quest 3 session and thousands of paired
Origins renders from caller RVA 00308607. This supersedes the earlier no-run
status. JJ's headset verdict reports HUD fusion and DLSS AA defects; it does
not accept the port. See BUILD_PLAN.md O1C for the recorded draw boundary.

STATIC movie receipt: startup-movie.json. GFullScreenMovie RVA 029c6ee4 is
assigned by c76d60; the GFx constructor bc50c0 assigns vtable 015dd410. Slot 5
points to bc64c0, thiscall stop(delay float, wait int, force int), RET 0x0c.
With wait=0 it executes InterlockedExchange(self+0x3c,2), clears self+0x7c,
and returns without cleanup. All 49 bytes are guarded with the IAT relocation
at byte 11. IsMoviePlaying reads the current FString at +14/+18 and finished
state at +40. These are Origins facts, not transferred City offsets. Installed
Startup.swf and bac_logo.usm/bac_logo_PC.usm/bac_logo_fra_PC.usm supply the name
allowlist. No MP or story movie is eligible. Live skip remains unobserved.

Looked up: City window.cpp opening-logo skip and failed forced EOF approach;
framework guide12 ??1?2; VR_HUD_GUIDE ??1?3,6; playbook INPUT-001/002, commit
b955f2244f7c5d5f3b3e5f403b1b6721a3362423. Fullscreen-triangle boundary and
startup control are separately supported by Origins LIVE/STATIC receipts.

## Reflection probe — 2026-10-11 (STATIC)

City's PERCAPTURE seam (City UpdateRequired 0x80A2B0, vtbl 0x1302414) located in
Origins by the same instruction shape (`re` scratch scan of `.text`):
FSceneCaptureProbeReflect-equivalent vtable **0x14BA744** (written by the
constructor at 0x7F74C1): [1] CaptureScene **0x7FDCF0**, [2] UpdateRequired
**0x7FDB90** (RVA 0x3FDB90; thiscall(probe, family+4), RET 4; only reference
is this vtable). Layout vs City: TimeBetweenCaptures +0x54 (City +0x4C),
LastCaptureTime +0x50 (+0x48), family game time +0x24 (+0x1C); CaptureScene
stores +0x50 = [arg+0x28] right after the vtbl[2] call. Origins-only: CaptureScene
exits early when global 0x18E6228 ("User/Reflections" system setting, string
0x15FE7AC) is 0 and probe+0xB0 != 0; it sets +0xB0=1 after a capture with the
setting off and clears it with the setting on — i.e. static one-time reflections
when the option is off; no effect on per-eye re-capture with it on. Hook guard:
`83ec1853558bd9837b3c00568b742428`. Live behaviour unobserved.
