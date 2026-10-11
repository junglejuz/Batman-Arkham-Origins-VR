# Batman Origins VR — implementation plan

Date: 2026-10-11 (Australia/Sydney). Status: **O1H built and installed following the VDXR/native runs**. Native default, startup skip, HUD capture fix, all saved DoF off and DLSS stock-AA cleanup are delivered. O1D/E/F failed transition headset checks. JJ confirms O1G bats returned but remained window-sized; paired live evidence shows generated masks with zero copied/submitted frames, so the original UI fallback was delivered. O1H corrects runtime texture transfer for TYPELESS RGBA and explicit typed conversion views. Actual GPU transfer/composite checks pass; O1H runtime/headset validation remains pending, along with HUD/DLSS visual acceptance. Current delivery: [README.md](README.md), [BUILD_PLAN.md](BUILD_PLAN.md), installed hash receipt `re/evidence/installed-build.json`. Historical discovery findings below are superseded by those build/results notes. Offline checks do not establish Origins stereo correctness.

**Looked up:** workspace `CLAUDE.md` imports, `VR_ESSENTIALS.md`, `SESSION_START.md`; `VR_HUD_GUIDE.md`, `VR_WORLD_MARKERS_GUIDE.md`, `PACKAGING_GUIDE.md`; framework guides 06–14 and 16 (relevant sections); playbook entry points, new-port/RE routes, stereo, projection, pacing, history and render-pass hazards; City handover, current plan/results, RE notes, DLSS plan and source; Knight `RESUME.md`, geo-11 plan/handoff/review and native handoff, current results and DLSS plan; native-stereo skill and architecture/visual diagnosis references; render-aspect/master RE references; improvement backlog and relevant memory records. Detailed source routes and conclusions are in §10. Playbook checkout: **`b955f2244f7c5d5f3b3e5f403b1b6721a3362423`**.

## 1. Intended result

Origins should feel and operate like the current Arkham City mod: a fully surrounding, head-tracked world; native stereo by default; the same controller gestures, settings panel, HUD placement, capture controls and launch conventions. AFW and DLSS are selectable alternatives for performance and image quality. Virtual Desktop/VDXR remains JJ's primary runtime; SteamVR is also a required supported runtime.

“Asynchronous field of view” is interpreted here as **asymmetric per-eye field of view**: each eye uses the headset's actual four lens angles. Asynchronous game/render threads and AFW have separate ownership and timing requirements below.

Initial gameplay scope: single-player campaign, third person by default, optional City-style first person, existing gamepad controls including Virtual Desktop Touch-as-gamepad. Match City's aiming, movement and camera options where Origins exposes equivalent actions. Physical hand combat, independently tracked gadgets and body-follow animation are later work, beyond parity with City's current controls. Cold, Cold Heart and challenge content enter regression testing after the campaign baseline, when available. Target the actual refresh rate selected by the player; make no 90/120 Hz performance promise before measurement.

Native means **two engine-rendered eyes from one simulation tick and one located head sample**. AFW is the explicit requested alternative: one eye rendered per game frame, with the other reconstructed. It must be labelled separately and must never silently replace native stereo.

## 2. Current evidence and first dependency

| Item | What is known | Evidence boundary |
|---|---|---|
| Local Origins installation | Installed at `E:\Games\Steam\steamapps\common\Batman Arkham Origins`; Steam app 209000, build 243683 | STATIC manifest/filesystem check following JJ's installation update |
| Target executable/build | `SinglePlayer\Binaries\Win32\BatmanOrigins.exe`: PE32 x86, 24,327,496 bytes, LAA already set; full SHA-256 and import/section findings in `RE_NOTES.md` | STATIC; engine layouts, hook ABI and live protection/load behavior remain unproven |
| D3D11 suitability | Shipped `BaseEngine.ini` and launcher settings enable D3D11; executable imports D3D9 and D3DX11 support | STATIC; configured/API support does not establish the active runtime renderer |
| City donor | Current source provides UE3 native stereo, AFW, OpenXR, panel/input, markers and an x64 DLSS helper | SOURCE; City results belong to City, not Origins |
| Knight donor | Native stereo, DLSS, turned-eye lighting fix, suit-gloss fix and SteamVR have headset evidence; AFW run has positive headset feedback | HEADSET on Knight; transfer to Origins is INFERENCE |
| Origins feasibility | City's architecture is the first candidate, with Knight's single-view-family clone as a second candidate | INFERENCE until target discovery and endurance gates pass |

Installation discovery and the initial static fingerprint are now complete. Read [RE_NOTES.md](RE_NOTES.md). Follow-up static work has mapped Origins' render-command boundary, candidate viewport root and camera constructor; focused Ghidra analysis succeeded for ten functions in an isolated project after GUI auto-analysis timed out. Origins uses different viewport slots and a 112-byte constructor argument layout versus City's 108 bytes. These findings refine the port; they do not prove live gameplay ownership or re-entry safety. Next: implement the passive `TEST PLAN OP1` caller/view census, then JJ's single-player run can batch actual renderer, configuration, checkpoint, mode and size evidence. No game launch, installation change or renderer hook has been performed. The x86 architecture is confirmed; City's addresses and layouts still do not transfer.

Origins-specific static additions to the risk register: imports include `GFSDK_ShadowLib.win32.dll`, `GFSDK_NVDOF_LIB.win32.dll`, `GFSDK_SSAO.win32.dll`, `GFSDK_PSM.win32.dll` and `GFSDK_GSA.win32.dll`; the install includes APEX turbulence/particles/cloth and GPU PhysX support. Audit which features actually execute and whether their contexts or simulation are shared across eye passes. The shipped base config uses `AspectRatio_MaintainYFOV`, whereas City's recorded setting used MaintainXFOV; verify the effective camera's policy rather than inheriting that assumption. Shipped frame-cap sections also differ and must be checked at runtime.

**JJ's chosen graphics baseline (corrected): DoF OFF and motion blur OFF.** DX11 Enhanced geometry, ambient occlusion and dynamic shadows remain the saved enhanced choices, with TXAA High and Hardware PhysX High. The current GSA/engine files still report DoF on; that is an unresolved saved-file discrepancy, not evidence overriding JJ's correction. Generated engine settings live in the installation; the GSA XML lives in Documents. Both originals are snapshotted with verified hashes; details in `RE_NOTES.md` “JJ's DX11 Enhanced baseline”. Start testing from JJ's reported DoF-off setup and verify the actual executing post chain. Compare enhanced features individually for stereo correctness and performance before selecting VR defaults.

## 3. Exact City conventions to preserve

Reference baseline: City source and latest results through 2026-10-11, especially `scene.cpp`, `afw.cpp`, `overlay.cpp`, `window.cpp`, launchers and installer. Older City plan sections are historical; its original “no SteamVR” exclusion is explicitly superseded.

| Convention | Origins requirement |
|---|---|
| Panel | Same visual treatment, controller navigation, saved settings and section order; FIRST PERSON at the top, then STEREO RENDERING, VIEW, HUD, MENUS AND SCREENS, ZOOM AND DETECTIVE VISION, VR RESOLUTION, DLSS (SMOOTHER EDGES), FRAME RATE; tests hidden behind `testtools` |
| Stereo labels | `Native (both eyes drawn every frame)` and `AFW (one eye drawn per frame, the other warped)` under STEREO RENDERING; matching “this frame”, “previous frame”, “both (best)” warp choices; combined is the proposed AFW default |
| Panel gesture | Hold both stick clicks for 0.6 s, once per hold; F10 keyboard equivalent |
| First-person gesture | Double-click both sticks quickly, using City's short-tap/hold distinction; retain the setting through menus while suspending its camera effect outside gameplay |
| Other keys | F12 recentre; F11 tracking toggle; F2 capture both output eyes, HUD, effective settings and frame/pose status |
| Input blocking | Block engine input while navigating the panel; swallow releases paired with swallowed presses. Cover Steam Input as well as XInput/DirectInput |
| Camera | Third person on first install; shoulder/height/forward offsets, world scale, lean and pitch behavior as in City. First person gated off for title/main menu, map/upgrades, pause, movies/loading and unsuitable scripted cameras |
| HUD | Room-fixed placement by default, per-group size/position/depth controls, markers at object depth; level panel/menu placement and readable dark backing |
| Resolution | Same 0.6×–1.8× control and restart indication, with requested/actual render and output sizes visible |
| DLSS | Same Off/Quality/Balanced/Performance choices, restart notices where required, texture sharpness controls and status; default chosen from Origins measurements |
| Frame rate | Same repeat-frame / runtime-reprojection choices and steady-rate lock; label Virtual Desktop SSW only on the runtime where it exists |
| Launch/install | Dedicated Origins VR and Origins VR (SteamVR) launchers; ordinary Steam launch remains flat; per-mode graphics profiles, preserved user settings and reversible uninstall |
| Identity | Provisional folder `batman_origins/`, application “Batman Origins VR”, mod `batman_origins.dll`, commands `bo ...`, settings `%LOCALAPPDATA%\Batman_Origins\`; independent from City and Knight |

Preserve the convention, not a hardcoded headset geometry: City's current resolution slider still uses a fixed 2064×2208 baseline. Origins must obtain its baseline from the **selected runtime's measured recommendation**, supporting SteamVR and other headset sizes without changing the familiar control.

## 4. Chosen architecture and native stereo proof

Start from an isolated adaptation of City's reusable core and UI, with an Origins-specific adapter, address registry and effect classification. Keep the City and Knight working trees/builds intact. Reuse source with its notices; do not run a second ReShade/Luma rendering layer or require geo-11 for native mode.

If the target is x86, use an x86 proxy/mod and x86 in-process OpenXR; use a separate x64 DLSS helper. City's actual loader is `xinput1_3.dll`, despite older diagrams mentioning `dinput8.dll`. Verify Origins' imports and load timing before choosing the proxy. If the shipping target differs, change the build/loader route before adding hooks.

1. Fingerprint executable/modules and inspect stock camera/view ownership. Locate UE native tables, local player, viewport root, scene-view constructor/allocation and render-command execution. Register signatures with build guards; unsupported or ambiguous matches disable engine edits and report why.
2. Count **live callers** before selecting the gameplay render root. City's first static caller guess was never executed. Confirm menu/movie/loading callers separately.
3. Primary candidate: City's caller-gated viewport-root replay, preserving the full scene-build/render boundary for each eye. Two internal eye presents may be necessary on this route; they represent **one simulation tick and one XR frame**, not two game frames.
4. Inject eye view, projection and view-state **at scene-view construction inputs** so the engine creates consistent inverses, translated matrices, frustum and culling. Use a cached original game camera and the same runtime pose for both passes. Restore original camera values so the game cannot read back and apply head movement twice.
5. Carry render-pass identity, eye, simulation/pair ID, runtime request ID, pose/FOV, camera matrices and resource generation through queued rendering. Game-thread globals and Present parity alone cannot identify async work correctly. Probe deferred contexts/command-list execution if Origins uses them.
6. Maintain independent temporal/occlusion history for each eye. Share adaptive exposure and once-per-tick simulation. Inspect special-mode state (detective/map) before substituting view states; Knight's `VSSPLIT` lesson shows that an empty custom state can lose mode effects.
7. Capture each completed eye before its targets are reused, and publish pixels plus render metadata atomically. Match the submitted pose/FOV to the pixels actually produced; never relabel an old image with a newer pose.
8. Prove simulation/input/animation/AI/audio/particles run once; world rendering runs twice. Check resource exhaustion and buffers overwritten before GPU consumption, then long soaks and level transitions.

If root replay fails the side-effect/endurance gate, investigate Knight's **two separate single-view families**, rendering the cloned world view before the game's own world+HUD path. Do not repeat HUD-bearing Draw blindly (Knight Scaleform crash), or assume two views in one family preserve postprocessing (ME/Knight failures). A genuine built-in two-view route can be investigated if Origins evidence supports it. No automatic fallback to plain alternating-eye stereo; retain the screen/mono diagnostic modes until native is proven.

## 5. Resolution, asymmetric lenses and frame pacing

Keep four dimensions/rectangles explicit: runtime-recommended output, chosen output scale, actual scene input/valid viewport, and desktop mirror. Logical movie/menu aspect is separate again.

- Query runtime view configuration sizes and limits. At output scale `s`, choose each output eye from its recommendation × `s`, rounded for required alignment. With DLSS, scene dimensions use the selected DLSS input/output ratio; without DLSS, scene and output dimensions match. Record padded allocations separately from valid pixels.
- Drive the **engine's render-size owner**, not just the OS window or DXGI allocation. Probe configuration/profile precedence, window-mode constraints, camera aspect constraints, internal viewport/scissor and actual depth/colour sizes. Do not resize every square target: shadow maps and reflection targets are separate resources.
- Keep the desktop mirror small, on the primary display and nonblocking. If the first donor implementation couples window and scene sizes, measure that explicitly and plan their separation; do not claim a larger buffer contains a larger scene.
- Gate stored geometry on a valid runtime measurement. Knight persisted a plausible placeholder square shape before the headset's real FOV arrived; simulator runs must back up and restore player settings.
- Build each eye projection from **that eye's four signed runtime angles and full pose**, preserving Origins' depth convention and near/far. Verify handedness, UE rotation/roll, matrix multiplication/storage order, units per metre and projection corners numerically; then prove near/far parallax and vertical alignment in captures/headset.
- Primary lens: true off-centre projection like City. Test lighting, shadows, fog, decals and culling at all lens edges. If Origins reproduces Knight's centred-lens assumptions, investigate a centred covering lens with an appropriately turned eye camera. Compute it from this runtime's frustum; **do not copy Knight's approximately 7° turn**.
- Any alternative lens needs coherent engine camera, culling, depth reconstruction and submission. For a turned-eye fallback under SteamVR, port Knight's straight-view rewarp and declare the corresponding straight lens/pose. Do not enable that correction for City's normal off-centre path.
- Runtime frame requests govern production: one wait/begin/end per complete XR frame, bounded request waits, coherent pair metadata and bounded queues. No extra desktop V-sync or stock FPS limiter fighting the session. Keep a controlled simulator cap.
- Measure engine/render/XR cadence separately. Under SteamVR, inherit City's shortest-valid-period pacer to avoid inflated-period slowdown, but reset/remeasure on session or confirmed refresh-rate changes. Keep repeat-frame metadata truthful, service runtime events and tear down cleanly on EXITING/STOPPING/LOSS_PENDING.

## 6. Shader, reflection and UI hazard register

These are **ranked transfer hypotheses**, not confirmed Origins defects. Capture shader identity, resources, constants, context, draw ordering and camera before patching. Use Origins-specific classifiers; no City/Knight shader hashes, register offsets or engine addresses transfer directly.

| Risk / visible symptom | Donor evidence and first discriminator | Origins candidate and acceptance |
|---|---|---|
| Puddles/reflections slide, stretch or differ by eye | City round 9 `PERCAPTURE`, HEADSET: reflection probe skipped right-eye update because game time was unchanged; `REFLECTEYE` texture shifting failed | Trace capture update predicates and mirrored camera. Refresh only identified eye-dependent captures per eye, preserve once-per-tick simulation; matched wet street/neon under head movement |
| Snow/rain/smoke/PhysX particles differ or advance twice | Knight `RAINONCE`/`FRAMESYNC`; fleet pause discriminator | Compare paused/running pairs and simulation counters; simulate once, render both eyes, share stochastic sequence. Test Origins snow explicitly and measure any PhysX mode separately |
| Suit rim/gloss or fake reflections differ per eye | Knight `SUITGLOSS3`, HEADSET: specific material shaders used camera direction rather than per-pixel eye direction | Inspect Origins shader inputs first; use head-free game-camera direction only for confirmed affected shader families. Preserve correct view-dependent material response elsewhere |
| Lighting/shadows vanish under an asymmetric lens | Knight `TURNEDEYE`, HEADSET; City accepts off-centre lenses | Test centred/off-centre lenses from the same scene; correct reconstruction/culling at source or use validated covering lens; equal-camera control distinguishes state leak from correct parallax |
| Exposure differs or temporal blur includes eye separation | Knight independent states/shared adaptation; STR-009, FAIL-STR-016 | Independent history per eye, common exposure. Measure identical-camera controls and shared bright surfaces, allowing real view-dependent differences |
| Objects pop/miss at edges | Playbook per-view decisions, FAIL-STR-025 | Verify each eye's frustum/history, share discrete LOD decisions where appropriate, diagnose occlusion queries; force-visible only as a measured diagnostic, not an unexplained shipping default |
| Disabling DoF loses grading/glows/AA/map | City `NODOF`; Knight DLSS plan/DoF results | Census the Origins post chain on/off. Keep the block if bundled; remove only proven blur steps, preserving tone mapping and mode effects |
| Flares, camera-centred fog/dirt or vignette cannot fuse | City vision/room overlay notes; Knight flare config alone was insufficient | Verify actual shader families, disable nonfusing flare families narrowly; room/sphere treatment for overlays when needed; verify zoom/detective/pass classifiers exclude tiny exposure/reflection targets |
| HUD flickers, compass masks/glows detach | City `HUDDSV`, Knight `HUDDSV` and marker glow follow-ups | Capture top-level HUD once, preserve private Scaleform filter passes, give it a correctly sized cleared depth/stencil; bind/clear redirection covers all paths. Match masks, lettering, backing and glow as one widget |
| HUD milkiness/banding or SteamVR additive glows missing | Knight `HUDINEYE`; City `SVRGLOW` | Per-eye HUD composition at real depth with appropriate blending; never enable colour/alpha writes on clip-mask draws. Runtime-specific alpha repair only if quad submission exhibits the known additive failure |
| Markers drift, turn with the head, split or float | City `MARKEREYE`/`MARKERBASE`; Knight's circle-cut and band/group failures | Read world anchors from Origins' projection calls; head-free reference, pair-matched per-eye card geometry, all widget pieces together. Exclude markers from HUD group moves and match at object depth |
| Tall eye buffers break loading/menu/map layout | City loading-band attempt failed; map `MAPSCREEN` restored the game-camera screen path; `FPGATE` prevents first-person menu takeover | Identify real UI/game mode, use logical 16:9 content on a room screen inside a full-FOV layer; gameplay/pause 360 world where valid. Separate HUD and map scene, never infer mode solely from stopped clock or frame counts |
| Mirror becomes a flat scene pasted over stereo | Knight `MIRRORLATE` / `MIRRORHUD2` | Draw mirror only after HUD capture closes; avoid adding HUD twice; saved HUD sheet must contain UI, not scenery |
| Our draw fails or destabilizes game state | City `BLITSTAGES`; Knight private-state blend and in-flight constant-buffer lessons | Save/clear/restore all used stages and bindings, including tessellation and compute; pooled versioned buffers must outlive GPU reads; no per-draw allocation/disassembly in release |

Apply VR comfort settings through the verified VR profile: motion blur, V-sync, camera shake, inappropriate lens effects and unsupported HDR output off. Preserve texture quality/anisotropy. Do not disable temporal AA merely because older AFR guides do; native history makes it usable, and DLSS supersedes only the proven AA pass. Keep source/game defaults untouched when generated profile edits suffice; preserve encoding, BOM policy and timestamps actually used by Origins.

## 7. DLSS implementation

DLSS here means **Super Resolution**, optionally DLAA where useful; frame generation is not required for this plan. Pin and record the tested SDK/runtime/model version instead of assuming “latest” is compatible. NVIDIA documents D3D11 support; the x86-helper decision comes from inspection of the actual DLSS payload and target architecture.

For an x86 target, adapt City's x64 helper/bridge (`helper64/`, `dlss_bridge.cpp`) and preserve versioned fixed-width shared metadata, same-adapter selection, resource generations and matching game/helper builds. For an x64 target, Knight's in-process route may remove helper overhead.

- Separate DLSS feature/history and previous camera per eye. In native mode both eyes use one jitter phase from the same simulation frame. In AFW advance a shared phase once per alternating-eye cycle, with each eye's history using its own previous **real** render and elapsed time.
- Capture valid colour and readable depth at each eye's final scene boundary, before HUD and after the game's intended grading path. Start with City's finished-picture approach; evaluate a pre-tonemap insertion only after Origins pass evidence justifies it. Declare colour/exposure/depth inversion accurately.
- Build jitter-free **camera motion vectors** from that eye's depth/current/previous camera as the proven starting point. This omits independently moving actors/cape/deformation: test combat, gliding and bosses explicitly. Investigate usable engine object vectors or masks only if measured ghosting requires them; do not advertise full motion-vector coverage.
- Preserve the post chain while bypassing only the identified stock AA resolve. City's FXAA draw was also a HUD boundary signal: do not remove a draw that classification still relies on.
- Texture mip bias must be idempotent: sampler replacements map to themselves and cover immediate/deferred binding paths. Compare full/mild/off bias for distant lamps/neon/snow shimmer.
- Render HUD/menu text at output resolution where the capture path allows it; DLSS does not process the HUD sheet. Captures save both final output eyes and depth/motion diagnostics on demand.
- Reuse City's `OVERLAP` shared-fence/CPU-wait design for two native eyes, with one bounded completion wait per frame and left-eye cleanup on every early exit. AFW requires one real-eye hand-off; handle it explicitly. Do not ship the old cross-process GPU-wait path associated with City freezes.
- Validate all acquired XR images are released on helper timeout/crash, mode switch, resize, session loss and shutdown. Use a temporary plain upscale on failure, clearly reported; restore native-size/no-jitter operation at the safe resize/restart boundary. Do not publish half pairs or mixed generations.
- Benchmark equal output size: native, lower-resolution plain upscale, DLSS Quality/Balanced/Performance and AFW combinations. Include helper waits, VRAM and VD encoding load. City Quality was slower than native; availability is required, a speed gain is not guaranteed. Proposed initial shipping default: Off until Origins establishes a useful, accepted preset.

City's 10-minute simulator `OVERLAP` soak is encouraging **LIVE/sim** evidence, not proof against the real VD-related freezes. Origins acceptance includes a 30-minute headset soak with the helper and VD active, longer than the previously observed fault windows, plus driver-event inspection.

## 8. AFW and SteamVR

### AFW

Port City's own D3D11 AFW, not the closed D3D12 AFW plugin. Its depth path already works without DLSS; preserve that independence in Origins once readable depth is proven.

One game frame renders left or right alternately. Reproject the fresh colour/depth into the other eye's **same-frame destination pose/lens**; combined mode fills disocclusions and outer-FOV gaps from that eye's previous real image reprojected to the current destination. Retain source age and camera metadata for the historical contribution, and reset histories on cuts, loading, recentre, geometry changes and native/AFW switches. Compose HUD/markers after the world warp so readable UI retains correct current-eye placement.

Expose the same three warp choices as City, but describe previous-only mode as historical reprojection. Display real L/R render rates separately from complete warped-pair rate, warp GPU time, hole/edge fill percentages and fallback counts. Feather source boundaries as in Knight's AFW result. Enable switches only at complete-frame boundaries; clear held native eyes and seed histories before changing cadence.

AFW cannot recover hidden geometry or correct view-dependent reflections/transparency from a depth surface alone. Specifically test close Batman/cape edges, snowy streets, steam, glass, lights and puddles while turning/running/gliding. At 90 Hz the combined pair may update 90 times/s while each eye is really rendered 45 times/s; it remains reconstruction and must be presented that way. Missing depth/warp support must disable AFW or enter a visibly labelled temporary screen fallback, with no stale eye passed off as native.

### SteamVR

Use OpenXR, with a dedicated launcher selecting the manifest **for that launch only**. For x86, validate the installed `steamxr_win32.json` and matching runtime DLL; search all registered Steam libraries. VDXR likewise needs a verified x86 runtime where applicable. No permanent global runtime switch and no additional OpenVR backend are planned.

Inherit City/Knight lessons: early DXGI 1.1 factory compatibility on the game device; correct adapter; SteamVR STAGE space when supported and validated, with logged fallback if unavailable; event/recentre handling; shortest-valid-period pacing; glow/alpha treatment for any quad HUD path; focus/audio handling at the actual game consumers. The 180° seated-universe problem was measured on JJ's setup, not a universal runtime property. True asymmetric eyes should use City's normal path; turned-eye fallback requires Knight's straight-view rewarp.

Require actual headset tests under **both VDXR and SteamVR**: orientation, fusion, tracking/recentre, brightness, additive glows, focus/audio, performance, map/menu return and clean exit. Simulator submission success cannot clear this gate.

## 9. Milestones and launch-efficient tests

| Milestone | Work / artifact | Exit proof |
|---|---|---|
| P0 — target and baseline | Locate single-player game; hashes/build/PE/API/imports/protection, config/profile stores, stock graphics/save/checkpoint; `RE_NOTES.md` | STATIC target fingerprint plus LIVE stock scene/API; originals backed up before any changes |
| P1 — donor foundation | Isolated core/Origins adapter, validated proxy, panel/input/captures, runtime screen and mode-specific launch | Intended DLL/config actually loaded; screen stable; Steam flat path unaffected; effective options logged |
| P2 — camera and lens | Main-view census, original camera, pose composition, actual scene sizing, asymmetric projection/inverses/culling | Numerical projection/transform checks and LIVE/sim near/far/edge captures; HEADSET stable scale/lean/rotation |
| P3 — native stereo | Validated root replay, alternative family probe prepared if needed, eye histories/metadata and side-effect instrumentation | 1 tick : 2 world renders : 1 XR pair; proper capture boundaries; no duplicated simulation; sustained soak/transitions clean |
| P4 — effects and full UI | Reflection capture refresh, snow/particles, lighting/history/DoF audit, HUD/markers, title/loading/map/upgrades/detective/zoom | Target-specific classifiers fire; both eyes visually coherent; no black UI rectangles, lost map, stale masks or marker splits |
| P5 — DLSS | Pass-through helper first if x86, then real per-eye DLSS/MVs/jitter/bias/full-size HUD | Correct depth/colour/motion registration; measured quality/cost table; failure cleanup clean; real-runtime long soak |
| P6 — AFW | Independent depth, alternating real-eye cadence, fresh/previous/combined warps, safe live switch | L/R render counts balanced, one complete pair per frame, recorded holes/age/cost, native comparison accepted |
| P7 — gameplay parity | City camera/first-person/input/comfort and mode gates; campaign traversal/combat/gadgets/bosses and available extra content | Third/first person both playable; camera suspended immediately outside gameplay; saves/reloads/transitions preserve settings |
| P8 — runtime/release | VDXR + SteamVR matrix, performance cleanup, installer/uninstaller/profiles/notices/README | HEADSET acceptance per runtime and supported mode; clean-install/update/uninstall restores exact originals; only tested claims in release |

SteamVR preflight begins at P1, with initial screen/lens checks at P1/P2; P8 is final regression, not the first SteamVR test. Native remains the reference throughout P5/P6.

Before **every** future game launch, write a numbered `TEST PLAN <id>` covering all currently answerable questions, controls, trigger counters and branches. Prepare all live switches and follow-ups in that build. JJ launches/navigates; tools trigger timed tests and record results. No need to type from the headset. One panel button runs bounded sequences, saves each pair/status and restores settings even on cancellation. Simulator first; check agent/session occupancy and other running games, back up settings and keep one capture folder.

### TEST PLAN O1 — first instrumented launch, after P0/P1 preparation

Commands below are **proposed interfaces**, not implemented commands. Enable rows only after their owning milestone's switches exist; do not put later-feature placeholders into a runtime test or pretend they ran. Stock baseline and manifest/bitness checks happen before O1.

| # | Command / switch | What it proves | How to judge |
|---|---|---|---|
| 0 | `bo mode screen`, `bo probe status`, capture | Injection/runtime/control | Identity/hash/runtime/device printed; screen contains current game pixels; no engine camera writes |
| 1 | `bo probe callers`, `bo probe views`, move camera | Actual render owner and seam | Raw caller counts plus camera/view/present relationships; distinguish movies, reflections and gameplay |
| 2 | `bo mode tracked`, `bo lens runtime`, capture/sweep | One-camera pose and projection | Expected movement/roll/scale, four FOV corners and viewport coverage; source and submitted lens agree |
| 3 | `bo probe state`, graphics menu then return | Config/aspect enforcement | Actual colour/depth/viewport sizes remain correct; trigger counters show which size owner was used |
| 4 | `bo test ui` | Controller blocking and mode signals | Panel hold/double-tap distinct, no gameplay action on swallowed release; menu/movie transitions classified promptly |
| 5 | `bo stereo burst <bounded count>` **last** | Native re-entry candidate | Per-frame tags, tick/eye counters, no second simulation; short burst is feasibility only |

Prepared branches: caller never fires → alternate root candidates from the same census; wrong scene size → log config/profile and engine viewport owners; asymmetric lights fail → centred covering-lens comparison; native immediate fault → disable replay, retain captures and use separately prepared family candidate only after static safety review. Any crash/rebuild leads to a new grouped plan.

### TEST PLAN O2 — native/effect acceptance, one session per runtime

Automated timed controls: identical-camera two-eye control → native near/far/head sweep → reflection update off/on at a wet surface → snow/particle pause/run → detective/zoom → pause/map/upgrades → return to menu/load → first-person glide/run/combat and suspend/resume → native soak. Every phase logs effective state and actual trigger counts. Fault-prone history/re-entry/mode stress goes last; restore the baseline after each comparison.

Prepared branches: reflection A/B has no effect → inspect actual mirrored capture camera/bind ordering instead of texture shifts; only moving particles differ → once-per-tick simulation probe; identical-camera eyes differ → shared state/pass scheduling before lens math; map only shows HUD → capture map scene before HUD routing, compare stock game-camera screen path; marker pieces separate → dump complete widget/mask/glow ownership and projection anchors, do not widen screen-band guesses.

### TEST PLAN O3 — performance, DLSS, AFW and stability

Hold output dimensions, scene, runtime/refresh and graphics options constant. Compare native without DLSS, plain smaller render, native+DLSS presets, AFW without DLSS, AFW+DLSS and native again. Batch live changes in one launch. Presets needing render-size restart are staged across the **minimum separate launches** with the same complete scene/test list; do not force unsafe live resize to satisfy “one launch”.

Measure new native/warped/repeated pairs separately; CPU/game/render times, per-eye GPU cost, warp/DLSS/helper waits, queue depth, latency and VRAM; inspect identical saved crops for trails, shimmer and edge holes. Exercise helper timeout/recovery and native↔AFW switching last. Perform sustained headset soaks with VD/encoding/helper in the loop and inspect driver events; then repeat the relevant matrix under SteamVR. An application FPS counter alone does not prove displayed headset cadence.

## 10. Reference register and delivery

The following were consulted for this plan. All playbook references use the checkout commit at the top; project-specific Origins notes stay here, outside the upstream checkout.

| Reference / section | Conclusion used |
|---|---|
| [Workspace essentials](../VR_ESSENTIALS.md) §§2–11; [session start](../SESSION_START.md) §§1–2 | Native default, measured lens/size, runtime pacing, runtime compatibility and batched tests |
| [City plan](../batman_city/BATMAN_CITY_PLAN.md) §2 and §10: round 9, STEAMVR/SVRPACE/HUDDSV, AFW1, MAP3/MAPSCREEN, LOADFIX, HEADCAM/FPGATE/STEREOSECTION | Most direct donor; use latest results, not old exclusions or initial status |
| [City source](../batman_city/src/game/arkhamcity/scene.cpp), `afw.cpp`, `window.cpp`, `markers.cpp`, `reflect.cpp`, core `overlay.cpp` and runtime | Exact current conventions and transfer boundaries; source is not Origins runtime proof |
| [City handover](../batman_city/HANDOVER_2026-10-04.md), [RE notes](../batman_city/RE_NOTES.md), [DLSS plan](../batman_city/DLSS_PLAN.md), [file changes](../batman_city/FIX_CHANGES.md) | Render-root caller census, x86 ABI, helper overhead/sync faults, OVERLAP evidence, config encoding |
| [Knight entry](../RESUME.md), [geo-11 plan](../akvr/GEO11_PLAN.md), [handoff](../akvr/CLAUDE_HANDOFF.md), [review](../akvr/PLAYBOOK_REVIEW.md) | Old AER boom-baseline failure, resolution/profile ownership, camera readback and pose matching; geo-11 route is historical donor evidence |
| [Knight native handoff](../akvr_native/HANDOFF.md), [notes](../akvr_native/NOTES.md): M3b/M4, HWDET, Run 16/17, SVR, SUITGLOSS, AFW1, FULLCUT crashes, PERF1/2 and marker follow-ups; [DLSS plan](../akvr_native/DLSS_PLAN.md) | Family-clone candidate, separate eye history, special-mode state, game-device filtering, lens/material problems, AFW edge feathering and avoid costly/unsafe per-draw work |
| [HUD guide](../VR_HUD_GUIDE.md) §§1–10; [marker guide](../VR_WORLD_MARKERS_GUIDE.md) §§1–6; [packaging guide](../PACKAGING_GUIDE.md) | Correct depth/blending/masks, room screens over world, complete marker widgets, preserved settings and reversible package |
| [Framework index](../references/vrframework/guides/00-README.md): 06 render size/OpenXR; 07 §§7–8; 08 §3; 09 §§5–6; 10 §2.4; 11 UI viewport; 12 §6; 13 new-engine discovery; 14 §4; 16 milestones 1–3 | Geometry/timing/submission/discovery recipes. Their AFR recommendations do not override this user's native requirement |
| [Playbook start route](../references/vr-modding-playbook/docs/start-new-port.md), RE route RE-01/02, AGENT_RE_WORKFLOW; [pattern catalog](../references/vr-modding-playbook/docs/pattern-catalog.md) STR-002/005/006/009/012/013/014 and PERF-007 | Fingerprint/ownership/exit proofs; atomic publication; shared adaptive state; render-thread identity. STR-013 is a culling lead, not permission to assume Origins multi-view postprocessing works |
| [Failure atlas](../references/vr-modding-playbook/docs/failure-atlas.md) FAIL-STR-009/016/025, FAIL-PERF-014/015/020, FAIL-HUD-014; [projection appendix](../references/vr-modding-playbook/docs/a3-stereo-projection.md) A3.1–A3.3 | Rendered/declared FOV, exposure, occlusion, stale pose, correct size, single HUD draw and projection validation |
| [Native stereo teardown](../references/vr-modding-playbook/docs/17-teardown-fc2vr-native-stereo.md) `side-effect-gate`, `reentry-endurance`; [hazard atlas](../references/vr-modding-playbook/docs/14-render-pass-hazard-atlas.md) temporal trap, `per-view-decisions`, `cbuffer-fields` | Double-tick/resource/overwrite hazards, previous-camera preservation and sustained acceptance |
| [OpenXR injection](../references/vr-modding-playbook/docs/09-d3d11-openxr-injection.md) `runtime-owns-pacing`, `wait-frame-is-not-a-pacer`; [performance](../references/vr-modding-playbook/docs/19-d3d12-and-performance.md) `dlss-in-vr`, `composed-reprojection` | Runtime pacing failure modes, truthful DLSS input registration, fresh depth warp plus historical hole filling |
| [Reference routes](../references/README.md), ME1 `STEREO_FIX_DOCUMENTATION.md`, BioShock Infinite `ENGINE_NOTES.md` native-root sections; [cross-project index](../references/vr-modding-playbook/docs/cross-project-index.md) relevant stereo rows | UE3 single-view postprocessing and correct replay boundary; current Batman evidence is stronger for this port |
| [Native-stereo skill](../references/native-stereo-vr-mods/SKILL.md) and architecture/visual-diagnosis references | Reuse transport/ownership methods, validate target ABI, keep logical screen aspect separate; never infer headset correctness from counters |
| [Backlog](../VR_IMPROVEMENT_BACKLOG.md) timing/pipeline items; [master RE hub](../RE_PLANS/VR_MODDING_MASTER_REFERENCE.md), [RE02](../RE_PLANS/RE02_RENDER_ASPECT.md); memory `ak-render-size-re-solved`, `afw-alternate-frame-warping` and related lookup hits | Measure bottleneck before optimizing. RE02 is explicitly historical/superseded; current native route and measured eye size supersede old resolution workarounds |
| [Official Origins store requirements](https://store.steampowered.com/app/209000/Batman_Arkham_Origins/?l=english); [NVIDIA DLSS integration](https://developer.nvidia.com/blog/?p=95492) | D3D11 is a reasonable candidate; NVIDIA supports D3D11 integration. Neither establishes the local target binary/ABI |

During implementation create `RE_NOTES.md` (target fingerprint, addresses/ABI, SOURCE/STATIC/LIVE/HEADSET receipts), `FIX_CHANGES.md` (exact external-file edit, backup, detect/undo) and per-milestone test/results sections. Capture source revisions, mod/helper hashes and effective configuration for every run. Negative/ambiguous tests remain recorded.

Release delivery: buildable isolated source, matched mod/helper, VR and SteamVR launchers, installer/uninstaller with independent flat/VR profiles, licenses/notices and a README reporting only validated behavior. Verify proxy identity before uninstall, config encoding preservation, reinstall/update, clean stock restoration and archive paths. No third-party fix or game artwork is assumed redistributable; HD loading artwork is optional later work, not required City interaction parity. Publish only when a release is requested.

**Current next step:** JJ's first run of the actual O1B VR build. It includes native stereo, AFW and the DLSS helper implementation, with live controls for the initial ownership gate. OP1/OP1B are superseded as standalone deliverables. Establish the gameplay caller from this run, then activate native in that same launch and collect O1B's controls and mode/capture evidence. Reflection fixes, first-person and complete City parity still require target work. No Origins runtime capability is claimed accepted before that evidence exists.
