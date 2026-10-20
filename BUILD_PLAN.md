## HEADLOCK - first-person head through the camera (JJ 2026-10-20, seen in Asylum 0.1.3)

Asylum's head read faulted every frame (unaligned matrix buffer, batman_asylum BUILD_PLAN HEADLOCK). Origins uses
GetBoneLocation (City's route, which works there with an unaligned vector); its out buffer is now 16-byte aligned too, to
be safe. Not yet run in Origins.
Fix 2, HEADLOCK (both games): the 0.08 s head smoothing may trail the real head bone by at most 3 UU (~3 cm), so a fast
lunge / glide cannot bring the head toward the camera (camera 25 cm in front of the face). Built, offline checks pass
(Asylum d3d9_gpu_transport intermittently fails at random stages while the GPU is 94% loaded by other apps - the released
0.1.3 DLL fails the same way now, it passed 3/3 this morning). Labelled 0.1.4, packaged, installed on JJ's game; NOT
released. HEADSET next: run + glide in first person; log must show "first person head: mesh ..., Bip01_Head - eye point"
and no "bone read faulted".

## RELEASE 0.1.3 (2026-10-20, JJ: "deploy both of those to github")

First person (FIRSTPERSON, below) + launcher access-denied fix (WRITEDENIED, below). Version 0.1.3 (CMake, package).
4/4 offline checks pass (new graphics_profile_write_denied); package zip sha256
cc13abb25cbd3d17a4140b624892b8e396da4a0e13af21abd3c60859b5e377fa, installed on JJ's game by the package installer. First
person HEADSET pending (shipped OFF by default).

## WRITEDENIED - same trap as Asylum's player report (JJ 2026-10-20: "check the origins trap first")

Origins keeps its graphics store in Documents (WB Games\Batman Arkham Origins\GFXSettings.BatmanArkhamOrigins.xml) and
the launcher wrote it every VR start. If Windows refuses that write (Controlled folder access blocks PowerShell in
Documents, permissions): the DoF step threw on every start, and a refused resolution write made the restore rewrite the
unchanged file, fail, keep profile-active.json and stop every later launch / install / uninstall - Asylum's exact bug
(batman_asylum BUILD_PLAN WRITEDENIED). Fix (SOURCE, tests/graphics_profile_check.ps1 PASS, new ctest):
1. Graphics-Profile.ps1: Test-OriginsWritable; Restore-OriginsProfile skips files equal to their original, continues
   past one failure, names each file + its saved original; Disable-OriginsDepthOfField takes no graphics store (engine
   only) and rolls back only what it wrote.
2. Launcher: write test before anything changes; unwritable engine file (game folder) stops with instructions; refused
   Documents store is left as it is with a warning (command line still sets window + size).
3. Installer: journal replay skips unchanged files and only warns; refused store -> engine-only setup + message.
   Uninstaller: replay skips unchanged files, stops before removing anything on a real failure; the final copy of the
   original store stops with instructions before the backup folder is deleted.

## FIRSTPERSON - City's first-person mode (JJ 2026-10-20: "add the same first person mode we have for the other arkham games ... add it to origins too")

Looked up: memory batman-city-first-person + ak-first-person; batman_city BATMAN_CITY_PLAN FIRSTPERSON / HEADCAM / FPGATE and scene.cpp fp_update / fp_head_follow / fp_apply (the donor); memory origins-port-city-fixes-by-code-shape (method); Origins never got first person (ORIGINS_PLAN P7 planned only). Playbook/framework: City already cites CAM-001/CAM-013, FAIL-CAM-020, guide 13 (REFramework FirstPerson) - nothing new needed for a straight port.

What it is (City's, unchanged): panel FIRST PERSON (top) or both sticks clicked twice quickly; camera at Batman's eye point
(Location + EyeHeight) following his Bip01_Head bone (smoothed 0.08 s, capped 140 UU), 25 cm in front of his face (head
not hidden), look direction stays the game camera's. Gate (FPGATE): an open menu (MENUSTATE: pause, map, upgrades), or the game camera more than 12 m from him (main
menu stand-in pawn) - the setting stays on. Saved in settings.ini [first_person] on / forward_cm / up_cm / head / head_smooth; the package ships it OFF. Commands `bo fp on|off|fwd N|up N|head 0|1|smooth S|status`.

STATIC RE 2026-10-20 (scratch scanner on the retail exe, City's functions found by identical code shape):
| | City | Origins |
|---|---|---|
| GetPawnViewLocation | 0x7A6970 | 0x7ACD50: Location +0x48, EyeHeight +0x2C0 |
| GetViewRotation | 0x7BA650 | 0x7B4A80: Controller +0x1E4 |
| Pawn.Mesh | +0x36C | +0x384 (game's own head read 0xF44E3B) |
| Head bone | GetBoneLocation 0x850EE0, FName 0x620 | FName ctor 0x485180 (L"Bip01_Head",1,1; ret 0xC) -> GetBoneLocation 0x84E590 (same signature, ret 0x10, SkeletalMesh +0x1C4) |
Every Pawn field is City's + 0x18 except Location.
Player -> controller (City +0x30) and controller -> pawn are found LIVE by the pawn's back-pointer (log line "first person:
player +0x.. = controller .., its +0x.. = Batman"). Hook: scene.cpp calc_detour (first eye pass) computes the eye point; ctor_detour moves `base` before the eye deltas (DLSS / AFW follow it).

Built + offline checks pass + installed via the player package (local build, not released). HEADSET pending.

### TEST PLAN FIRSTPERSON (JJ drives, one launch)
| # | do | look for | log |
|---|---|---|---|
| 1 | VR shortcut, main menu | normal menu 3D (first person must NOT move the menu camera even if on) | "first person paused: ..." |
| 2 | gameplay, both sticks clicked twice quickly | view jumps to Batman's eyes, cowl not in view | "player +0x.. = controller", "head: name Bip01_Head", "eye point" |
| 3 | walk, run, jump/glide, a fight/takedown | camera stays at his head, no shaking, not inside his head | head frames rising, "steady eye point" count low |
| 4 | pause menu / map, then back | menu as before, first person back after | "paused: menu" / "active again" |
| 5 | panel: untick "camera follows his head" | steadier but head comes through camera on glides | - |
| 6 | anything wrong: F2 | capture | - |
Follow-ups: pawn never found -> widen the live scan (player range 0x20-0x80, controller 0x100-0x700); head misses -> log names the
reason (no mesh / no skeleton / no bone / cap).

# Origins playable development build — O1B

## TEST PLAN O1C — opening-title skip and HUD fusion (before build/live changes)

User requests 2026-10-11: gamepad button skips beginning titles; HUD elements
are too far apart to fuse. Origins is now running under VDXR. Preserve this run.

| # | stage / switch | proves | judge / branch |
|---|---|---|---|
| 1 | `bo status`, `bo capture` during reported problem | active route and eye images | compare screen/native ownership and per-eye UI; JJ identifies affected screen |
| 2 | HUD alignment live controls where supported | asymmetric-lens versus quad-depth cause | retain runtime lens data; no engine writes from Present; request JJ fusion judgment |
| 3 | startup skip build + offline input lifecycle | fresh pad edge stops only startup player | held-before-start does not skip; release consumed across title/menu transition; story movies pass through |
| 4 | build and install after JJ exits | deployable title/HUD change | compile, existing checks, exact target guards and manifest hashes |
| 5 | next JJ-driven launch: face/Start/Back click at logos, menu, gameplay HUD, story movie | actual skip and fusion | logs/captures plus JJ headset verdict; no autonomous launch |

Looked up: City window.cpp opening-logo skip/failed EOF approach;
framework guide 12 §§1–2 (XInput transport); VR_HUD_GUIDE §§1–3,6;
playbook pattern catalog INPUT-001/002 (mapping, press-owner release), checkout
b955f2244f7c5d5f3b3e5f403b1b6721a3362423. Origins movie seams require target
static guards; donor addresses are not used.

User steering 2026-10-11: build an actual VR implementation; a passive probe is
not the deliverable. OP1B is superseded as a standalone build. Diagnostics belong
inside the VR implementation.

Looked up: City current core, native scene adapter, AFW and DLSS bridge/helper;
Origins focused Ghidra receipts and x86 stack cleanup; VR_ESSENTIALS §§2–11,
framework guides 08/09/10/13 and playbook a3/a4, 17 side-effect-gate,
STR-002/009/012 and FAIL-PERF-014/020, checkout
b955f2244f7c5d5f3b3e5f403b1b6721a3362423. Reuse transport and graphics code;
reconstruct every engine address/layout on Origins.

## Implementation

Build an isolated x86 mod and XInput proxy, City-derived OpenXR renderer and
panel, Origins native viewport replay and constructor adapter, AFW compute
reconstruction, and a separate x64 DLSS helper. Ordinary launch stays flat.
Guard executable SHA256 and relocated instruction signatures. Engine camera
inputs use Origins' 28 stack words. Do not include donor engine address tables
or shader patches. Camera/projection inputs are validated numerically before
editing. Unknown mode/effect behavior remains explicitly unvalidated.

## TEST PLAN O1B (before build or launch)

| # | switch/stage | proves | judge / prepared branch |
|---|---|---|---|
| 0 | build x86 + x64; foreign-host load | real binary dependencies and host guard | compile/link, PE exports/bitness, no initialization on foreign host |
| 1 | screen/default; title + gameplay | real game D3D11 and headset session | log/session/captures; no root hits -> remain on screen, expose census |
| 2 | `bo caller <RVA>` then `bo stereo on` | native root route from confirmed live caller | two views with same pose generation, render-thread labels and pair counters; screen rollback available |
| 3 | `bo cam off/on`, `bo asym off/on`, `bo hud off/on` | camera/lens/UI correctness | eye captures + JJ headset; compare each control in same launch |
| 4 | wet street, paused/unpaused snow; detective/map/menu/load | effect/lifecycle hazards | no shader identity transfer; capture faults/skips and restore screen control |
| 5 | `bo afw on/off`, DLSS presets | compute reconstruction/helper integration | actual warp/evaluate counts and separate per-eye history; failed depth -> retain native |
| 6 | native soak; menu quit | re-entry endurance and clean runtime exit | JJ drives; no autonomous game launch |

Runtime caller selection is live/configurable, not guessed from a donor address.
Initial game run is still required to establish the gameplay owner. Include
the actual stereo implementation in the first build so this gate can be cleared
and stereo enabled during that same launch without rebuilding a probe.

DoF OFF and motion blur OFF remain JJ's authoritative baseline; installer does
not change either setting. Keep enhanced geometry/AO/shadows. Back up/restore
graphics stores around launch size changes. Record SOURCE, build/offline, LIVE
and HEADSET independently.

## O1B RESULTS — build/offline and deployment, 2026-10-11

**SOURCE/BUILD:** MSVC Release x86 batman_origins.dll, xinput1_3.dll and runtime-
size query built; x64 bo_dlss64.exe built with the DLL CRT required by the local
NGX library. Private core/AFW/DLSS donor copies compile with the Origins adapter.
The donor projects and their installations were not rebuilt or modified.

**STATIC:** all five 16-byte target signatures match the fingerprinted EXE;
relocation-bearing DWORDs are adjusted at runtime. Allocator `009d4920` allocates
0x350 bytes, calls `009d2180` with ECX=allocation, and returns with no stack
arguments; `007d554a` calls it and stores EAX at local-player+0x7c. This differs
from City's 0x390 state. Receipt: re/evidence/view-state-allocator.json.

**OFFLINE:** both CTest checks passed. The actual Origins constructor detour
forwarded ECX, EDX, all 28 words and the result once across 4096 calls; asymmetric
lens corners mapped to ±1 and preserved depth terms. Foreign host rejected before
framework initialization; ordinary proxy load did not load the VR DLL. Four
PowerShell scripts parsed. These checks do not prove retail-game ABI semantics,
safe re-entry, graphics correctness or headset behavior.

**DEPLOYED:** 14 manifest-owned files installed and hash-verified at
`E:/Games/Steam/steamapps/common/Batman Arkham Origins/SinglePlayer/Binaries/Win32`.
The EXE retained its recorded SHA256. BmEngine.ini and GSA XML equal their
first-install snapshots byte for byte. No launcher or game was started by the
agent. Build/deploy receipts: re/evidence/build-checks.json and installed-build.json.

**LIVE/HEADSET: pending.** First run still has the gameplay-owner gate; the real
stereo implementation is already in the DLL and can activate in that same run.
First-person, zoom/markers, reflection and vendor-effects fixes, complete City
parity, deferred-context coverage and runtime/headset acceptance remain work.

## O1C RESULTS ? 2026-10-11

**LIVE O1B:** VDXR Quest 3, game framebuffer 1524x1634, eye outputs 2284x2448
(DLSS Quality). Origins caller RVA 00308607 produced over 4,000 native pairs
with no tag misses/pair aborts in the sampled run. This establishes that route
in the Cell Block A checkpoint, not every game mode or endurance acceptance.
JJ reported unfusable title/menu/gameplay HUD and jagged AA.

**LIVE diagnosis:** saved overlay capture 14584671 and diagnostic 14552859
show identical HUD pixel coordinates in asymmetric eye images. The recorded
scene resolve is non-indexed Draw(3), no DSV, full viewport, sampling a rounded
1528x1640 HDR RT; 32 subsequent backbuffer UI draws follow it. The copied City
DrawIndexed(6) rule never found this boundary. Receipt: O1B-hud-boundary-framedump.txt.

**SOURCE O1C:** native defaults to the observed caller; startup/non-view roots
retain screen handling. Non-indexed triangle boundary is adapter-opt-in and
retains viewport/no-depth/RT-SRV checks. HUD capture ownership moves with the
render-thread eye tag, not the asynchronous game-thread setting. HUD sheet is
4.2 m wide at 4 m (within binocular overlap), per-eye quads retained; size/depth
controls save in settings.ini. Captures now include hud_panel.bmp.

Startup skip uses Origins' 49-byte guarded stop body, exact GFx vtable/global,
current Startup movie name, unfinished state and observed known logo filename.
The non-waiting stop only exchanges its state and resets its queue flag; it
performs no renderer cleanup or virtual calls. Decoder reads are unmodified.
A fresh face/Start/Back edge is consumed through release. Buffered-file closure
does not prematurely end the eligible movie; story/MP names do not qualify.
Runtime skip execution/latency are pending.

**BUILD-OFFLINE:** x86 mod/proxy/query and x64 helper compile. Three CTests pass,
including actual HUD classifier/redirect on a hidden WARP swapchain with scene
draw left untouched, startup name/edge/release ownership and existing foreign-
host/constructor/lens/ordinary-launch checks. Full static movie guard matches.
DoF profile fixture verifies both BOM states, duplicate keys, unrelated options
and idempotence. PowerShell scripts parse.

**DEPLOYED:** 15 manifest-owned files hash-verified on E:. DoF GSA=OFF, every
DepthOfField/NVDOF=False, DisableHighQualityDOF=True; backup receipt records prior
files. Only requested DoF values changed in the installed graphics files. When
DLSS is selected the launcher disables game AA/TXAA/MSAA/MLAA temporarily; other
graphics settings restore on exit. NGX helper ABI v5 reports actual evaluated
mode; overlap output counts are separate per eye and from copy-only output.
The misleading route-off text is corrected.

**HEADSET O1C: pending.** The DLSS change removes competing stock AA but is not
a verified cure for jagged edges. Existing camera-only motion vectors lack
animated-object velocities; quality, reset/history behavior, menus/maps, story
movies and runtime-specific HUD acceptance still need the grouped next run.
Evidence: O1C-build-checks.json, startup-movie.json, installed-build.json,
DoF-off-installed.json; project-relative re/evidence directory.

## TEST PLAN O1D — bat transitions, one JJ-driven launch

Looked up: City BATCUTS/STAGEBOX/WIPEHD/MAPSCREEN: preserve title art and map
shapes; use authentic animation at higher raster resolution. VR_HUD_GUIDE
sections 1/6/7/10: full projection FOV, own matching stencil, premultiplied alpha,
VD per-eye placement. Playbook HUD-004: separate-alpha capture and a kill switch;
checkout b955f2244f7c5d5f3b3e5f403b1b6721a3362423. Donor identities do not transfer.

1. Build/offline: retain the three O1C checks; exercise the real transition
   redirect on WARP, including a larger output RT, viewport/scissor
   restoration, untouched constant buffers, event expiry, unrelated textures/map rejection and rollback.
2. Default control: native + current DLSS Quality; bat transitions enabled,
   distance 0.8 m, enlargement 1.6. No City offsets, wipe recordings, hide-all
   or map timing rules. Exact `Play_UI_Fader_Bats` audio event opens a bounded
   candidate window; post-resolve small vector/RT draws are candidates, not
   established Origins draw identities until the next live receipt.
3. JJ: title → main menu → Continue → gameplay; pause → resume; pause → main
   menu → quit. Check the bats fill the view, are sharp and fuse; check title
   artwork, map geometry, labels and prompts retain their normal size.
4. Overlay BAT TRANSITIONS: enabled checkbox, distance and enlargement sliders;
   timed original-view control restores automatically after 15 seconds. This
   provides both prebuilt follow-ups without typing in the headset or rebuilding.
5. Proof: exact event counts, candidate/redirect counts and actual capture size;
   F2/overlay capture also saves transition RGBA and the HUD rolling timeline.
   An event alone does not establish successful draw classification or headset
   acceptance. If a context is silent or uses a different event, retain the
   original draw and use the capture/timeline to extend the target receipt.
6. Recheck HUD fusion, native pair/tag/abort counts, actual per-eye NGX delivery,
   startup skip and DoF OFF in this same launch. Do not launch/kill the game
   autonomously. Build + deployment are distinct from LIVE and HEADSET proof.

References: City BATMAN_CITY_PLAN BATCUTS, STAGEBOX perspective clip-column fix,
WIPEHD, MAPSCREEN (lines 611–616, 741–845, 1200–1237); VR_HUD_GUIDE sections
1, 6, 7, 10; playbook docs/pattern-catalog HUD-004 at checkout
b955f2244f7c5d5f3b3e5f403b1b6721a3362423. City findings are SOURCE leads only.

## O1D RESULTS — 2026-10-11

**STATIC:** Origins exports all three Wwise PostEvent overloads. Their decorated
ABI is cdecl, with eight DWORD-sized slots including the 32-bit GameObjectID;
the runtime guards export RVAs and body prefixes and forwards all arguments and
the return unchanged. Origins GetIDFromString uses lowercase ASCII FNV-1:
Play_UI_Fader_Bats = 0xe8bb4d2c. A1E57F61.bnk has an actual HIRC type-4 event
with that ID. Frontend's CFX at virtual 0x4f26e6 contains the shared Fader,
bat event, Intro/Outro, FE_PressStart and FE_UnPause names. These facts establish
the event and asset lead, not the final GPU draw identities or all contexts.
Receipt: re/evidence/O1D-transitions-static.json; read-only reproduction:
python batman_origins/re/transitions.py. UEViewer's UnPackage3.cpp supplied a
chunk-layout lead; actual Origins summary records and block sizes are checked.
The exact chunk array contains 77 Frontend chunks (11 Startup), correcting the
preliminary 75-chunk Frontend analysis buffer. Only the O1D-prefixed analysis
buffers are current; installed packages remain untouched.

**SOURCE:** a 2.5-second exact-event window admits small indexed post-resolve UI
draw candidates with no texture or 64/256-square textures. Counts >=2000,
non-indexed draws and large title textures are excluded. Actual prior Origins
map observation was 4962 indices. This is a bounded candidate rule, not final
shader identity proof. Colored captured content returns to its ordinary UI
placement; only near-black coverage enters the close layer. Constant buffers
are untouched. The viewport/scissors scale for real supersampled game draws;
own matching-size depth/stencil mirrors clears and uses the game's depth-clear
convention. Full runtime projection FOV is retained. SteamVR uses physical-eye
origins; VDXR receives the guide's explicit per-eye placement correction.

Defaults: enabled, 0.8 m distance, 1.6 enlargement. Actual raster size is output
eye size times enlargement, rounded up, capped at 8192; output coverage is the
full headset eye size. Allocation/shader/unsupported-depth failure leaves the
original candidate draw; preparation failure disables the path and restores
raw captured UI. A silent/different event context retains the original path.
Sliders persist in settings.ini [bats]; original comparison restores after 15 s.
Next-transition recording captures raster, coverage, HUD, eye images, metadata
and rolling HUD census at candidate frames 1/8/20 under LOCALAPPDATA. This
recorder intentionally stalls for disk readbacks; it is opt-in. Eye BMPs alone
exclude XR quad layers, so use the coverage and HUD files to assess those.

**BUILD-OFFLINE:** x86 build passes. All three CTests pass; foreign_host_guard
also runs the actual transition module on WARP, verifies a 154x167 raster for
96x104 output at 1.6, original viewport/scissor restoration, separate stencil
with unchanged source, black alpha coverage, colored UI preservation, unrelated
large texture/map/non-indexed rejection, event expiry/disable and 3072 calls
through the three real forwarding detours. No game was launched for these.
These checks do not exercise a real XR compositor or prove bat shape sharpness.

**DEPLOYED:** 15 manifest-owned files installed and independently hash-verified
on E:. Installed identity O1D. Engine/GSA graphics stores remain byte-identical
to the existing DoF-off baseline. Prior 15 O1C files and its manifest are saved
under re/backups/O1C-installed/. Helper ABI v5 remains unchanged. Receipt:
re/evidence/O1D-build-checks.json, O1D-ctest.txt, installed-build.json.

**LIVE (prior O1C):** preserved O1C-last-live.log ends with 5299 native pairs,
0 tag misses, 1 abort; actual NGX deliveries 5297/5296, copy-only 0/0; 11501
HUD boundaries, 92239 captured UI draws. It proves those observed routes only.
**LIVE/HEADSET O1D: pending.** JJ should relaunch once and check title/main-menu
entry, Continue/game entry, pause/resume, quit-to-menu and exit. Use the timed
original control and next-transition recorder during that same run. Confirm
event AND redirected draws, fill/sharpness/fusion, preserved titles and map.

## TEST PLAN O1E — remove the bat-transition window

Looked up again: City STAGEBOX/WIPEMAIN3/WIPEPANEL and VR_HUD_GUIDE sections
6/7/10 distinguish source window, stale HUD and actual projection coverage.
Framework guide 11 section 1 requires translating the source UI rectangle;
playbook 04 #hud-cropped-not-scaled and HUD-004 distinguish source crop from
output size/alpha. Checkout b955f2244f7c5d5f3b3e5f403b1b6721a3362423.

HEADSET O1D failure: all four contexts still confined to small windows. LIVE
log shows four exact bat events, real 3655x3917 raster and XR layer creation,
then permanent feature disable at 13:09:01.435 when the gameplay-loading switch
temporarily removed the ordinary UI target. Untextured 2103/5514-index shapes
occur during exact bat events; O1D wrongly excludes >=2000 as map geometry.
An output-size increase preserves source-stage margins and cannot remove them.

1. Fix now: normalize near-black coverage's actual source bounds on GPU to the
   whole output layer, preserving colored UI placement. During the exact event
   admit large untextured transition fills and 1/4-pixel fill textures; retain
   ordinary map/scene behavior outside the event. No permanent disable from
   transient flat/loading UI-target loss; independent mask preparation and a
   current-swapchain fallback for colored UI. No CPU bounds readback in runtime.
2. Offline: real black rectangle surrounded by transparent pixels must cover
   output corners; transparent/colored pictures stay transparent in the mask.
   Exercise four separate event cycles and a redirect-off/loading target reset,
   matching stencil, viewport/scissor restoration and shader/resource state.
3. Build/install O1E while Origins is closed; preserve O1D log/files and hashes.
4. One JJ-driven run: first main menu, Continue into gameplay, quit gameplay to
   menu, quit game. Compare timed original vs enabled and record next transition
   from the panel. Judge full-view black/bat coverage, title/map preservation,
   sharpness/fusion and later-context counters after loading. Check native/DLSS
   in the same run. Agent does not launch or kill games.

## O1E RESULTS — 2026-10-11

**HEADSET O1D:** JJ reports small windows for first main-menu entry, gameplay
entry, gameplay exit and game exit. This is a failed coverage result, superseding
the prior pending status. **LIVE O1D:** O1D-windowed-transitions.log contains
four exact event signals and real raster/layer allocation, but index-count
filtering excluded event-owned 2103/5514-index fills. Preparation failure when
the ordinary UI target was absent disabled the path for the rest of the run.
Output resolution/layer creation never established full source-content coverage.

**SOURCE O1E:** near-black premultiplied coverage bounds are reduced on GPU,
using group-local reduction and four global atomics per group. The actual
covered source rectangle, including perspective-stage placement, maps to the
whole output layer; source-window margins no longer determine the visible size.
Transparent gaps survive, and colored candidate UI returns at its original
placement. This deliberately changes the source window's scale/aspect as its
covered extent animates; check animation stability and peripheral bat shape in
the headset. No CPU bounds readback or new disk capture occurs by default.

Large indexed untextured fills and 1/4-square fill textures are now admitted
only within the exact-event window; large title textures and non-indexed scene
draws remain excluded. The 2000-index map assumption is removed. The bounded
window is now 6 seconds: the observed post-loading fade at +4.4 s fell outside
O1D's arbitrary 2.5 s. Loads beyond this measured interval remain an ownership
question for the next run; do not treat this timeout as a proved loading API.
No event-free map takeover occurs. Mask preparation is independent of the
ordinary UI destination; a current-swapchain target restores colored UI during
loading. A transient failure restores that frame and retries, rather than
permanently disabling every later transition. Periodic status now includes
event/candidate/draw/prepared-frame/failure counts. Existing distance, zoom,
enabled, timed original comparison and opt-in recorder remain available.

**BUILD-OFFLINE:** x86 Release compiles; all three CTests pass. Real WARP
shader/module checks render four synthetic small source-window fixtures and
verify every output corner receives black coverage. Internal transparent gaps,
colored UI, source stencil preservation, viewport/scissor restoration, CS SRV
restoration, expiry/disable and the original cdecl forwarding checks pass.
The actual loading failure is reproduced by removing the game RTV and disabling
HUD redirect: independent mask preparation and swapchain fallback pass. Large
fills work during the exact event and remain unchanged without it. These are
synthetic route fixtures, not four game-context captures or compositor proof.
The corner test caught a structured-UAV initialization error; initializing all
four bounds fields through a 16-byte upload fixes it. Receipt: O1E-ctest.txt.

**DEPLOYED:** O1E installed on E:, 15 manifest-owned files independently
hash-verified. Prior O1D files/manifest preserved in re/backups/O1D-installed/;
both O1D run logs retained. Engine/GSA graphics bytes unchanged. No game was
launched or killed by the agent. Receipts: O1E-build-checks.json and
installed-build.json. **LIVE/HEADSET O1E: pending.** Test the four requested
contexts in one JJ-driven launch, especially the post-loading fade and final
exit. Sharpness/fusion and the dynamic crop's animation stability need review.

## TEST PLAN O1F — restore animated bats, black before the first menu

Looked up again after two failed headset fixes: playbook 06 "Dump every stage"
and cross-project-index, City WIPEFLIP/WIPEPANEL/BLACKFILL/MENUFADE, and
VR_HUD_GUIDE sections 6/7/10. Checkout b955f2244f7c5d5f3b3e5f403b1b6721a3362423.
The dynamic bounds approach is rejected: an opaque animated fill becomes fully
black immediately, destroying the intended wipe. The initial window also
precedes the exact sound event, so event-only capture cannot own initial black.

1. Replace O1E crop/capture with a full-frame recorded animation, using JJ's
   private City BCWP v2 asset (1440x1536, 70 frames, 1172 ms, SHA256
   066715fb726f93674e48a1649ba4f46ddb284041f786ceb8c519ea1eec2f5eb5).
   This is explicit animation reuse, not Origins shader/ABI identity transfer.
   Read-only asset/RLE validation and a contact sheet precede implementation.
2. Logos and the 3D title prompt remain visible before the first bat event.
   First exact title-exit event: full opaque black, then inverted animation opens bat
   gaps revealing the main menu. Later events: visible cover animation,
   bounded hold over loading, then reveal when actual projection returns.
   Suppress the ordinary HUD/window while owned, with clean restoration.
3. Head-relative full-FOV transition layer avoids room-fixed rectangle edges.
   Keep native scene projections unchanged. Own the replay independently of
   per-draw candidate size/stencil; no dynamic bounding of black geometry.
4. Offline: reject corrupt RLE, check real asset frames vary over time, full
   initial/final black and transparent endpoints, cover/reveal direction,
   no erase from a small opaque square, loading state, timed original control,
   original audio forwarding and ordinary flat-launch guards. Prebuilt capture
   records replay frame/direction/mask plus HUD timeline for the next run.
5. Build/install O1F while closed; preserve O1E files/settings/logs. Private
   captured animation stays local; no publication. One JJ-driven run: logos ->
   full black -> bat reveal -> main menu; Continue cover/load/reveal; pause exit
   cover/reveal; final quit cover. Also check ordinary HUD/native/DLSS after each.

## O1F RESULTS — 2026-10-11

**HEADSET O1E:** JJ reports gameplay entry/exit bats were skipped and first
title-to-menu reveal remained a square. This supersedes O1E's pending verdict.
**LIVE O1E:** saved `re/evidence/O1E-skipped-transitions.log` records exact
events #1/#2/#3, loading camera/cinematic switches and excessive candidate draws
after gameplay exit. Successful allocation and counters did not prove the
animation survived. Dynamic black-region bounds were the wrong operation:
opaque animated geometry fills the remapped output immediately. This route is
removed from the Origins transition module; core legacy helpers are unused here.

**Looked up again:** playbook `docs/06-debugging-methodology.md` "Dump every
stage at the same instant" and `docs/cross-project-index.md`; City plan
WIPEFLIP/WIPEPANEL/BLACKFILL/MENUFADE; `VR_HUD_GUIDE.md` sections 6/7/10;
framework guide 11 UI lanes and playbook HUD-004. Checkout
`b955f2244f7c5d5f3b3e5f403b1b6721a3362423`. These support separating the
coverage layer, first-menu inversion and suppressing the old UI during replay.
They do not establish Origins addresses or shader identities.

**STATIC ASSET:** JJ's saved City `menu_wipe.bin` validates as BCWP v2,
1440x1536, 70 frames over 1172 ms, SHA256
`066715fb726f93674e48a1649ba4f46ddb284041f786ceb8c519ea1eec2f5eb5`.
The inspected contact sheet contains moving bats, internal gaps, and an opaque
ending. Reports: `O1F-wipe-recording.json`, `O1F-wipe-contact.png`. This is
explicit private animation reuse, not native Origins artwork extraction or
donor-engine offset/ABI transfer. No public package or upstream update is made.

**SOURCE:** Origins' original guarded Wwise exports/hash and all eight cdecl
arguments/return values are retained. Exact events now drive a pure replay
state machine. The title-exit event starts with opaque full-view black, followed
by inverted bat coverage. Later events play cover -> black hold -> reveal;
handoff uses the actual per-present projection verdict after cinematic fallback,
with 350 ms of continuous projection before reveal and a 60-second watchdog.
Animation starts at presentation, rather than consuming time while render/load
is blocked. RLE storage remains compact; only the current coverage frame is
decoded/uploaded. No runtime bounds readback or source-stage scaling occurs.

The prior live log shows projection active roughly 3 seconds before event #1.
Therefore projection alone cannot identify title exit. The initial plan to
blacken any first projection was corrected before installation: pre-event
projection/title prompts remain visible. The first presented exit frame is
explicitly black even if it arrives late after the sound event.

During replay only the proved post-resolve UI lane is skipped and the separate
HUD capture is cleared; the normal HUD layer is omitted while coverage is
provided. Existing scene and title/movie drawing remain intact. UI restores
when ownership ends. Missing/invalid assets or failed resource setup retain
the original UI. The coverage quad uses view space and both eyes' full FOV,
with existing VDXR eye offsets and SteamVR physical-eye semantics. Normal native
projection remains unchanged. Initial black is exact; the last 120 ms blend to
an exact clear/opaque endpoint so recorded transparent margins cannot persist.
Existing saved distance 0.8 m / enlargement 2.30, DLSS Balanced and HUD settings
were preserved byte-for-byte. The opt-in recorder includes replay phase, source
frame, elapsed time, direction and layer coverage; eye BMPs exclude XR quads.

**BUILD-OFFLINE:** x86 Release and all three CTests pass. The WARP module test
loads the actual private animation, verifies frames differ, retains bat gaps,
renders complementary cover/reveal, checks black at every corner/centre and
clear/opaque endpoints, and restores both VS/PS constant-buffer state. Parser
checks reject truncated/zero-run/nonincreasing/trailing data. State checks cover
title visibility, delayed first-frame black, first/menu reveal, three later
cover/load/reveal cycles, transient projection and watchdog restoration. Timed
original/disabled modes preserve original drawing; 3072 calls verify original
audio forwarding. Existing HUD boundary, constructor/lens, startup input,
foreign-host and flat-launch checks pass. Receipt: `O1F-ctest.txt`. These tests
are actual-asset offline rendering and synthetic state timing, not four
observed game transitions or compositor/headset proof.

**DEPLOYED:** O1F installed on E:, all 16 owned files independently hash-verified,
including `OriginsVR/bat-wipe.bin`. Prior 15 O1E files/manifest plus user settings
verified and preserved in `re/backups/O1E-installed/`. Game EXE, Engine/GSA stores
and user settings are unchanged. Helper/shared ABI v5 is unchanged. Receipt:
`O1F-build-checks.json`, `installed-build.json`. No game launched or killed.
**LIVE/HEADSET O1F: pending.** One JJ-driven launch should cover first title exit,
gameplay entry, gameplay exit, final quit and HUD/native/DLSS restoration.

## TEST PLAN O1G — preserve the menu beneath the bat animation

**HEADSET O1F failed:** JJ reports bats still missing. Saved live log
`O1F-missing-bats.log` proves the recording loaded, exact events fired and replay
ran; it also records 545 hidden UI draws during the first reveal. This does not
prove compositor visibility. The previous WARP test checked coverage in isolation,
not the image visible through its holes.

**Looked up again:** playbook 06 "Dump every stage at the same instant" and
cross-project-index, HUD-004 in pattern-catalog (capture/composite alpha),
framework guide 11 "The core problem", VR_HUD_GUIDE sections 4/6, City
WIPEFLIP/WIPEPANEL/BLACKFILL and the later MENUARRIVE results. Checkout
b955f2244f7c5d5f3b3e5f403b1b6721a3362423. A coverage matte cannot reveal content
removed by an earlier hide rule. City-specific all-UI suppression is a lead,
not evidence that Origins' scene/UI split behaves the same way.

1. Remove the all-UI draw veto/clear. Capture original UI normally, then remove
   only its near-black transition coverage from the sheet during replay. Submit
   the preserved coloured menu/HUD underneath the full-view bat coverage.
   Missing resource/setup must preserve original drawing. No stage geometry,
   cdecl hooks, stereo or graphics-setting changes.
2. Offline test the complete GPU composite using the actual bat recording and
   a UI fixture containing black stage coverage plus visible menu content.
   The old blanket skip must fail that fixture; black coverage must disappear
   from the UI sheet while coloured content survives and is revealed by bats.
   Check render state, cover/reveal endpoints and actual UNORM-to-sRGB copy.
3. Add bounded automatic paired captures during the first two transition events
   so the next run records raw UI, filtered UI, mask and XR-copy status together.
   Keep the existing opt-in recorder for later events. Distinguish a rendered
   mask, successful XR copy, submitted layer and final headset visibility.
4. Build/install O1G while closed, preserving O1F files/log/settings. One JJ-driven
   run: title exit, gameplay entry, gameplay exit, final quit; inspect menu/HUD
   restoration and the same run's evidence rather than asking for live typing.

## O1G RESULTS — 2026-10-11

**HEADSET O1F:** JJ says the bats are still missing. This supersedes pending.
**LIVE O1F:** `re/evidence/O1F-missing-bats.log` proves asset loading, first
exact event/reveal, a 2284x2448 layer allocation and 105 generated first-reveal
frames. It also records 545 discarded UI draws during that reveal. Event #2
plays cover and enters hold during the observed cinematic/loading switch.
No paired UI/mask captures or accepted-layer counters existed in O1F. Therefore
its actual failure stage is not conclusively localized by this log; generated
frames and a ready swapchain were insufficient evidence. Do not call the new
source correction a headset-confirmed root cause.

**SOURCE O1G:** the blanket post-boundary UI veto and capture clear are removed.
All original bat/menu draws follow the normal Origins capture path. During
replay the HUD provider returns a second sheet containing coloured content,
with only near-black coverage removed using the existing premultiplied colour
split. The regular per-eye HUD quads are submitted beneath the full-view bat
layer. Thus content inside bat-shaped openings is retained, and the old black
UI backdrop does not cover it. Filtering is gated on the actual successful XR
coverage copy; failed setup/copy retains the untouched source UI and its bats.
The mask now supports shader-resource binding for non-RGBA runtime-format
fallbacks, and the XR swapchain declares transfer-destination usage.

Counters separate generated coverage, successful copy, and bat-bearing frames
accepted by xrEndFrame; setup/copy failures are named in the log. One sample at
>=400 ms of animation from each first two real events saves raw HUD,
preserved_menu.bmp, transition_coverage.bmp, phase/source/time and submission
counts in `%LOCALAPPDATA%/Batman_Origins/bat_layers_*`. Later events retain the
opt-in recorder. Layer images remain distinct from the eye BMPs; these captures
do not themselves reproduce the runtime's final headset composition.

**BUILD-OFFLINE:** all three CTests pass. The actual bat recording is GPU-rendered
over a synthetic coloured-menu fixture with an opaque near-black stage. The
stage alpha is removed while menu colour/coverage survives; the final GPU
composite contains both hidden and revealed menu regions through bat-shaped
gaps. The inspected `re/evidence/O1G-composite/visible_bat_reveal.bmp` shows those
openings. O1F's source-level blanket suppression would discard the fixture's
menu before this composite; this is an explanation of that regression, not
an observed O1F headset capture. The copy into a real WARP sRGB-family texture
preserves mask alpha. Existing parser/state/endpoints, VS/PS constant buffers,
original mode, audio forwarding, HUD boundary, constructor/lens, startup input
and flat-launch guards pass. Receipt: `O1G-ctest.txt`. No fake or real game was
launched by the agent.

**DEPLOYED:** O1G installed on E:, 16 owned files independently hash-verified.
Previous O1F files/manifest/settings saved and verified in
`re/backups/O1F-installed/`; the failed run log is retained. Engine/GSA stores,
settings.ini, source recording and game EXE remain unchanged. Helper/shared ABI
v5 and audio/engine hooks are unchanged. Receipts: `O1G-build-checks.json`,
`O1G-before-graphics.json`, `installed-build.json`. **LIVE/HEADSET O1G: pending.**
The next single JJ-driven run covers title reveal, gameplay entry/exit and quit,
with automatic paired evidence to distinguish menu removal from copy/submission
or actual headset presentation failure.

## TEST PLAN O1H — deliver the full-field animation to runtime images

**HEADSET O1G:** JJ confirms bats returned but remain window-sized.
**LIVE:** preserved log and both paired captures in `re/evidence/O1G-small-window/`
show generated=36, copied=0, submitted=0 at the first sample; periodic status
later reports 98 generated and 98 failures. Both events have coverage images
but no preserved-menu image. The fallback original panel animation is therefore
the only bat layer delivered. Allocation success did not establish copy success.

**Looked up again:** playbook 09 "Swapchain images come back TYPELESS, so name
the view format" (#typeless-swapchain), framework guide 10 sections 1/2.4
(runtime-owned image copy and acquire/wait/release), VR_HUD_GUIDE sections 6/8,
and City's WIPEMAIN3/MENUFADE. Playbook checkout
`b955f2244f7c5d5f3b3e5f403b1b6721a3362423`. Existing eye_rtv already supplies
an explicit typed view; the new transition fallback does not. Leading source
candidate: a typeless RGBA image misses the compatible-copy check, then a null
RTV descriptor fails. O1G logs do not contain the returned texture descriptor,
so this specific live cause remains an inference until the new diagnostic runs.

1. Share the runtime texture transfer with the WARP checks. Direct-copy matching
   RGBA families including TYPELESS; format-converting draws use explicit typed
   RTV/SRV descriptors. Keep original UI/bats on failure and successful-copy
   gating of menu filtering. Do not replace the animation or change its timing.
2. Report actual requested/returned format and failure stage/result, including
   acquire/wait/release; partial swapchain allocation must be cleaned up.
3. Test the actual transfer helper on typed sRGB, typeless RGBA and typeless BGRA
   runtime-shaped images. Check coverage alpha and black endpoints on the GPU.
   Retain existing title/gameplay playback, audio, input, lens and flat guards.
4. Build/install O1H after backing up all owned O1G files and settings; verify
   every installed hash and unchanged game/graphics/settings bytes.

One JJ-driven launch covers these checks together; no agent game launch.

| # | command / switch | what it proves | how to judge |
|---|---|---|---|
| 1 | default bats on; leave title | full black then full-field reveal | auto paired capture; copied/submitted >0; headset has no small bat window |
| 2 | enter gameplay, return to menu, quit | all real event contexts use full-field coverage | event count, accepted frames, preserved UI and normal HUD restoration |
| 3 | `bo bats original` / `bo bats on` | fallback still contains original bats | original lasts 15 s; next event returns to replacement |
| 4 | `bo bats record` before another event if needed | follow-up evidence without relaunch | raw/preserved/coverage captures plus format and failure-stage log |

### O1H RESULTS

**SOURCE:** runtime transition transfer now shares `blit::copy_rgba_layer`
with the GPU tests. A returned RGBA8 TYPELESS image is accepted for compatible
CopyResource; conversion to another family names the typed RTV and SRV formats.
The runtime logs requested/returned formats, bind flags, acquire/wait/release
failures and conversion HRESULTs. Partial allocation failures are cleaned up;
copy readiness requires successful release. All-UI suppression remains absent,
and UI filtering still requires a successful coverage handoff. Full-FOV quad
geometry, animation, trigger/timing, saved size/distance and native/DLSS settings
are unchanged. The actual O1G resource format remains unobserved, so TYPELESS
is a source-backed candidate, not a live-confirmed descriptor.

**BUILD-OFFLINE:** all three CTests pass (`re/evidence/O1H-ctest.txt`). The WARP
fixture reproduces CreateRenderTargetView(nullptr) failure on real TYPELESS
RGBA/BGRA images, then successfully exercises the actual runtime transfer
helper for typed sRGB, typeless RGBA and typeless BGRA. Readbacks preserve the
recorded bat mask's shapes/gaps/alpha, opaque black at every corner/centre and
the transparent reveal endpoint. Existing menu composite, audio forwarding,
playback/loading, graphics state, HUD, startup and ordinary-launch guards pass.
No game was launched or killed.

**DEPLOYED:** O1H installed on E:; all 16 manifest-owned files independently
hash-verified. Previous O1G files/manifest/settings backed up and verified in
`re/backups/O1G-installed/`. Game EXE, Engine/GSA graphics stores and settings.ini
are byte unchanged. Receipts: `O1H-build-checks.json`,
`O1H-before-graphics.json`, `installed-build.json`. Helper/shared ABI v5 and
private recording are unchanged. **LIVE/HEADSET O1H: pending.** Next run must
show copied/submitted >0 and full-field visible bats, rather than inferring
success from a generated mask or ready swapchain.

## TEST PLAN O1I — puddle reflections per eye (PERCAPTURE port)

JJ 2026-10-11: ground-plane reflections show the same problem Arkham City had.
Looked up: VR_ESSENTIALS §2 (reflection captures update once per game time;
refresh per eye); City `BATMAN_CITY_PLAN.md` round 9 REFLECTEYE / PERCAPTURE
(shift attempt had no effect; per-eye capture fixed it, JJ "well done, you fixed
it") and `src/game/arkhamcity/reflect.cpp`; ORIGINS_PLAN §6 hazard row "Puddles/
reflections"; playbook 17 #side-effect-gate, checkout
b955f2244f7c5d5f3b3e5f403b1b6721a3362423. Same cause expected; Origins'
address found statically (RE_NOTES "Reflection probe").

**SOURCE O1I:** `src/game/origins/reflect.cpp` hooks Origins' reflection-probe
UpdateRequired (RVA 0x3FDB90, own 16-byte guard). BeginDrawing labels each pass
with its eye. In the right pass, a probe that updated in this frame's left pass
answers "update" again, so it renders mirrored from the right eye. Panel: VIEW →
"Puddle reflections rendered for each eye" (saved `[view] reflect_per_eye`,
default on). Command `bo reflect on|off|status`; counters in every 5 s status.
**BUILD-OFFLINE:** three CTests pass. **DEPLOYED:** installed on E:,
batman_origins.dll SHA-256 150124B5…AAA856 hash-verified. **LIVE/HEADSET: pending.**

One JJ-driven launch; can share the O1H bat checks.

| # | command / switch | what it proves | how to judge |
|---|---|---|---|
| 1 | log at start | hook installed | "hook reflection capture update check RVA 003FDB90: MH_OK" |
| 2 | gameplay at a wet street / puddle with neon; status | probe runs in both passes | left-pass captures >0 and right-pass re-captures rising at about the same rate; log "first right-eye re-capture" |
| 3 | `bo reflect off`, F2; `bo reflect on`, F2, same spot | the fix is what changes the right eye | off: right-eye reflection shifted/skewed vs left; on: both eyes match. JJ headset: reflections stay put on head turns |
| 4 | 10 min of play incl. a load | no fault, no frame cost spike | no crash; frame rate within a few fps of off |

Prepared branches: re-captures stay 0 while left captures rise → the right pass
does not call the probe (check pass labelling / which family renders captures);
left captures 0 → the Reflections option is off (game flag 0x18E6228 = "User/
Reflections"; probes then capture once and stay static) or the puddles use
another probe type (find its vtable the same way); re-captures rise but no
visible change → compare `bo reflect off/on` captures pixel-wise, then check
whether the ground samples a second (e.g. screen-space) reflection.

### O1I RESULTS (2026-10-11 14:12-14:15, JJ in the headset, VDXR, native + DLSS mode 4)

1. **LIVE:** hook installed (`hook 1`); left-pass captures and right-pass re-captures rise
   together (851/851, 886/885), right skips 0 while reflections update.
2. **LIVE:** right skips rose only while the left pass did not update either (14:13:35-14:14,
   just after a controller rebinding = JJ away/paused): no capture in either eye, harmless.
3. **LIVE A/B** (`bo reflect off` + F2 → captures/48814250; `bo reflect on` + F2 → 48824281):
   re-captures frozen at 1714 while off, resumed when on. With the fix on, the pink/red
   ground reflections match in both eyes; off, they differ (camera moved between shots).
4. **HEADSET:** JJ "I can tell immediately it looks fixed". **PERCAPTURE accepted in Origins.**
   Soak (#4) not separately timed; no fault during the session.

## TEST PLAN DB1 — crash opening the database (Back button), JJ 2026-10-11 14:28

Symptom (HEADSET/LIVE): pressing the controller's left menu button (database) → game's own "Fatal
error" box, addresses 0xC76B8C / 0xC7756C / 0x41EFA1. STATIC: 0xC76B8C is the faulting read inside
the game allocator's Free (0xC76B60, binned allocator: handle → page table, VirtualFree), called by
the thread-safe wrapper 0xC77510 and appFree 0x41EF80 (GMalloc 0x17EE798 vtbl+0xC). = a free of an
invalid pointer (double free or earlier heap damage); the caller of appFree was NOT reported (the
game's stack walk stops there). Our crash filter never ran: the game handles the AV itself. The DLSS
"right eye answer missing" at 14:28:14.504 is the same moment (most likely a consequence).
Looked up: City plan §5 risk 2 "re-entry instability (BL1 heap crash, ME main-menu crash)" and the
menu/movie/loading gate; VR_ESSENTIALS §2 (AKN: a draw doing HUD twice crashed in Scaleform);
memory/notes grep for database/fatal/heap - nothing else; web search - no known stock crash with these
addresses.

Ranked causes: (1) PERCAPTURE right-eye re-capture (new in this build; database never opened in
earlier runs) - a probe torn down/created as the menu opens; (2) the doubled viewport draw running
menu-opening work (Scaleform movie load/close) twice; (3) right-eye view state used with a menu
view; (4) unrelated stock bug.

**SOURCE DB1:** `core/util/crash.cpp` GAMEAV: a first-chance access violation inside the game
module is reported (minidump in `%LOCALAPPDATA%/Batman_Origins/crash/`, registers, 24 code
addresses from the stack incl. our DLL) - up to 4 per session. Installed, hash-verified.

| # | command / switch | what it proves | how to judge |
|---|---|---|---|
| 1 | gameplay; `bo reflect off`; JJ opens database, browses, closes | cause 1 | no crash → re-capture implicated |
| 2 | (if 1 passed) `bo reflect on`; open database again | reproduces with the fix on | crash → GAMEAV log names the appFree caller |
| 3 | (if 1 crashed) read GAMEAV stack | which subsystem freed | caller in Scaleform/movie → cause 2; in scene capture → 1; view state → 3 |
Follow-ups prepared: cause 1 → skip right-eye re-capture while the menu clock stands / for probes
first seen this frame; cause 2 → no doubling while a menu movie opens (City MENUCLOCK signal);
cause 4 → test once with `bo stereo off`.

### DB1 RESULTS run 1 (14:36-14:39, JJ drove without the planned `reflect off`)
- LIVE: puddle fix ON, database opened/browsed/closed with **no crash** → crash is intermittent;
  cause 1 not supported by this run. GAMEAV logging stays armed for the next occurrence.
- HEADSET (JJ): in the database a flickering **duplicate of the screen on the left-hand side**.
- LIVE: captures 50213203/50213953/50273078: both eye pictures black (scene behind the menu not
  drawn), the menu complete in the HUD sheet (1328x1424, MAP SYSTEM page). Stereo doubling ran every
  frame (root/second +56 per second, equal); cinematic (flat) quad only 14:37:08-10 while opening;
  menu placement never switched (Origins never calls set_hud_menu_now). So during the menu the
  headset gets a black projection + the HUD sheet as a per-eye quad pair at 4 m.
- Open: what the duplicate is. Candidates: (a) VD showing both per-eye HUD quads to each eye
  (VR_ESSENTIALS §6 SCREENQUAD2 / PANELONE: "per-eye pair under VD likely doubles"); (b) the
  sheet content alternating between passes; (c) the 2 s flat fallback when opening.
  Live A/B available now: `bo hud off` (menu drawn straight into the eyes, no HUD quads).

## TEST PLAN MT1 — skewed screen copy flickering on the left in the database

JJ 2026-10-11: "might be a bit skewed", present the whole time the database is open. Looked up: DB1
analysis above; VR_ESSENTIALS §6 (VD per-eye quad pair; menus), §1 "put A/B sequences in the mod".
**SOURCE MT1:** panel HUD → "Menu flicker test (40 s, open the menu first)" / `bo menutest`: four
10 s steps, capture at 5 s each, log `[bo] MENUTEST step ...`, restores everything; step 1 also saves
8 consecutive HUD sheets to `captures/menutest_sheets`. New live switch `bo hudeyes 0|1`.
Installed, hash-verified.

| # | step | what it proves | how to judge |
|---|---|---|---|
| 1 | normal (control) | copy visible; consecutive HUD sheets differ? | JJ sees copy; sheet sequence alternates → pass-alternation |
| 2 | one HUD copy for both eyes | per-eye quad pair is the copy | copy gone (menu flat/at screen depth) |
| 3 | AFW (one eye drawn per frame) | the second render's HUD pass is the copy | copy gone |
| 4 | menu drawn into the eye pictures | the HUD layer is the copy | copy gone; captures show the menu in the eyes |
Follow-ups: 2 → use one quad for menus (HUDMENU signal from the map-screen detector); 3 → draw the
HUD sheet once per pair; 4 only → inspect HUD layer placement / re-anchoring on sheet gaps.

### MT1 RESULTS (15:36-15:37, run twice by JJ in the headset)
- HEADSET (JJ): **none of the four steps removed the copy.**
- LIVE: all steps logged and restored. Control/step 2/step 3 eye pictures are EXACTLY black (0 non-black
  pixels); step 4 eye pictures each hold one clean menu; 8 consecutive HUD sheets identical (diff 0.0-0.03).
  Bats layer idle (generated stays 577), cinematic quad only while opening, no mono presents (2 presents
  per pair), letterbox verdict only acts in flat-screen mode.
- Conclusion: what we submit is one menu (quad or in the eyes) over black; the skewed copy is not in our
  eye images or HUD sheet. Remaining: another of our layers (panel, laser/aim dots, vignette), or the
  compositor/Virtual Desktop (e.g. its frame synthesis/reprojection on an all-black frame).
- Next: a Quest screen recording of the database (shows the composited headset view), plus: does the
  copy stay fixed in the room or move with the head; is it also in the pause menu.

### MT1 follow-up — cause found: EARLYUI (2026-10-11)
JJ: SSW is off (frame synthesis ruled out); the copy is "outside what the game drew". Projection layer is
opaque (no alpha flag). **LIVE (log arithmetic):** eye renders vs scene boundaries found per 5 s: gameplay
0 missed; in the database 2-20 missed per 5 s (~2-4 eye passes/s). In a pass with no boundary the menu is
not redirected and is drawn into that eye picture at screen pixels (head-locked, off-centre lens) = the
flickering skewed copy; matches the 25/1500 gaps in the HUD frame record. Captures never caught one
(they sample single frames). Step 4 of MT1 put the menu in the eyes every frame, so it could not remove it.
**SOURCE EARLYUI** (core `gfx_hud.cpp` classify): in an eye pass (redirect on), a backbuffer draw before
any scene blit starts the UI run provisionally; if the scene blit still arrives in that pass, the capture
is cleared and the blit goes to the eye (stock-equivalent: the full-screen blit covers earlier draws).
Switch `bo earlyui on|off`, panel HUD checkbox, default on. Status line: passes caught / blit came later.
BUILD-OFFLINE: 3 CTests pass. DEPLOYED, hash-verified. **HEADSET: pending** - open the database: copy gone;
status "caught" rises only in menus; gameplay HUD unchanged ("blit came later" may count, harmless).
- **EARLYUI v1 HEADSET (JJ 16:55): black headset entering the game from the menu.** LIVE: during the
  load 164 presents per 5 s came WITHOUT an eye pass (loading screen) while the redirect flag still held
  the last eye pass's "on": EARLYUI caught 162 of them = the loading screen went into the HUD sheet. In
  gameplay it caught 6, all undone by the later blit (correct). **v2 SOURCE:** EARLYUI also requires an
  open eye pass (`gfx_hud::note_eye_pass` from BeginDrawing, cleared at Present). Built, CTests pass,
  installed hash-verified. HEADSET pending: loading screen visible, database copy gone, gameplay HUD same.
  Lesson: a per-pass decision must key on a per-pass flag, never a sticky one.

## TEST PLAN MT2 - map grid still flashing outside the map window (EARLYUI v2), JJ video 2026-10-11

Looked up: MT1 / EARLYUI notes above (same symptom, same session); VR_ESSENTIALS §6 (HUD redirect; one
decision per pass); the video `temp/Origins_Map_Flicker.mp4` (Quest recording of the map, 33 s).
**HEADSET (video):** the flash is NOT a copy of the whole menu: single frames show a large full-view
cyan/red grid to the left of the map window, nothing else (frame at ~10 s). About 13 visible flashes in
33 s (the 30 fps recording sees one in three headset frames, so ~1/s). **LIVE (log 16:57-16:58):** in the
map, EARLYUI "caught" rose ~3/s and "blit came later" ~1-2/s; in gameplay "blit came later" is ~0.
**Cause (SOURCE):** the map's 3D grid is a backbuffer-size render target drawn by one full-screen quad,
so it passes `is_boundary_blit`. In a provisional pass, that quad was taken as "the scene blit came
after all": the menu pieces so far were cleared and the grid went into the eye picture at screen
pixels (head-locked, off-centre lens) = the flash.
**SOURCE EARLYSTRICT** (`gfx_hud.cpp`): the scene blit's source picture is learnt from ordinary passes
(blit before any UI, ring of 8, each new one logged); the undo needs that same picture; any other
full-screen picture stays on the menu sheet (logged up to 8). `bo earlystrict on|off`, panel HUD
checkbox, default on; status adds "full-screen pictures kept as UI". BUILD-OFFLINE: 3 CTests pass.
DEPLOYED: hash-verified, dll SHA-256 CBBB97BD...5D3DA2. **HEADSET: pending.**

| # | command / switch | what it proves | how to judge |
|---|---|---|---|
| 1 | gameplay 20 s, status | learning works, gameplay unchanged | "scene blit source picture" logged (1-2 pictures, eye size); "kept as UI" stays 0 |
| 2 | open the map 30 s | the fix removes the flash | JJ: no grid outside the map window; "kept as UI" rises ~1-2/s, "blit came later" stops rising |
| 3 | `bo earlystrict off` 15 s, then `on` | the switch is what changes it | off: flashes return; on: gone |
| 4 | database, pause menu, a load, back to gameplay | no side effects | loading screen visible, HUD normal, no scene picture on the menu sheet |

Prepared branches: flashes remain while "kept" rises → a second escape route (the grid as the FIRST
full-screen draw of a pass = learnt as "scene"): check the logged source list for a second picture
during the map and require the scene picture to be the one seen in gameplay; "kept" stays 0 in the map →
the map grid uses the scene's own buffer: then discriminate by blend state / pixel shader instead.

### MT2 RESULTS run 1 (17:09-17:10, JJ, HEADSET: "still happening"; City has a similar leftover)
- LIVE: only ONE scene source learnt (1328x1424 fmt 10); in the map "blit came later" still rose (3 -> 8)
  with "kept as UI" 0 = the late blit samples the scene's OWN picture. The map grid is rendered into the
  scene picture and blitted again; source identity cannot separate it. Negative result recorded.
- **SOURCE EARLYSTRICT v2:** in ordinary passes a second scene-picture blit after the boundary marks a
  map-type screen (counter "second scene blits"); for 1 s after one, OR while the MAPSCREEN detector says
  the map is up (big untextured shapes, 1.5 s hold, now shared), a late scene blit in a provisional pass
  stays on the menu sheet (reason logged). BUILD-OFFLINE 3/3 CTests; DEPLOYED hash-verified. HEADSET pending.
- Judge: map 30 s, no flash; status "kept as UI" rises in the map, "blit came later" stops rising there;
  "second scene blits" > 0 in the map, 0 in gameplay; `bo earlystrict off` brings the flash back.
- Branch: kept rises but flash remains → a third route; next tool: a per-pass log of every backbuffer
  full-screen draw in the map (source, blend, order) for 2 s.
- City (no EARLYUI at all): port EARLYUI v2 + EARLYSTRICT v2 once Origins is confirmed.

### MT2 RESULTS run 2 (17:21-17:22, JJ HEADSET: "still flickering outside the window"; JJ: "did you reference the City fix?")
- LIVE: "second scene blits" 4-6 total (none while the map was steady), MAPSCREEN up from 17:22:04, and only ~1
  undo in the steady map, yet the flash stayed = the undo route is NOT the main cause. Two fixes failed ->
  looked up again (SESSION_START rule).
- **Looked up again:** City BATMAN_CITY_PLAN "PANELMAP / MAP2 / MAP3 / MAPSCREEN / MENUOPEN / OPENFADE" (City's map
  flicker "outside the bounds" was transition rules sending untextured map pieces into the eye; City has no EARLYUI);
  Origins log: zero "wipe starts" lines in the map (those rules did not fire here); status: tag-miss 0, root =
  second and begin = presents in the steady map (both eyes, every pass labelled); DLSS input = the backbuffer at
  present (dlss_bridge ov_send), so DLSS only carries what is already on the backbuffer; playbook
  symptom/failure grep "single-frame flash / stale input": nothing relevant.
- Conclusion: no counter identifies the route; stop patching blind. **SOURCE LEAKTRACE / MAPTRACE**: panel HUD
  "Map flicker recorder (20 s, open the map first)" / `bo maptrace`. 3 s delay, then one line per present
  (eye pass, sign, events B boundary / P provisional / U undo / K kept / S second blit, draws to sheet / hidden /
  eye / pre-boundary eye, the eye-bound draws listed with count and texture size) and an objective eye check:
  four full-width strips of each eye picture read back (lit pixels in the map = leak), first 4 leaking eye
  pictures saved whole. Steps: normal 10 s, EARLYSTRICT off 5 s, EARLYUI off 5 s, restore; log leaks per step.
  Files `captures\maptrace_<time>\leaktrace.txt` + `leak_p*.bmp`. BUILD-OFFLINE 3/3; DEPLOYED.
- Branches: leaks = 0 while JJ sees flashes -> not our eye picture (compositor / another of our layers: check
  the room screen / cinematic quad / bats layer timing); leaks > 0 -> the leak line names the draw (pre:/eye:)
  and its decision events -> fix that route; the saved bmp shows what leaked.

### MAPTRACE RESULTS 17:37 (JJ pressed the recorder on the map) -> MAPBLIT
- LIVE (objective): 176 of 4998 eye pictures leaked (63 in 10 s normal, 40 with EARLYSTRICT off, 41 with EARLYUI
  off = those switches do not matter). ALL 176 in eye sign 1. In every leaking present all HUD draws went to the
  sheet (eye 0, pre-eye 0): the only eye draw was the boundary scene blit. Saved pictures (colour only): the
  map's grid background with its blue glow (lit ~8870 px in the strips) and the floor-plan shape (lit 1992).
  = the map art is drawn into the shared scene picture and sometimes is already there at this eye's FIRST
  blit (left by the other eye's pass), so it went full-view into the eye. The trace's "eyepass" column reads
  0 always (written after the flag is cleared: recorder artefact, ignore).
- **SOURCE MAPBLIT:** at the boundary, while MAPSCREEN says the map is up, the eye is cleared black and the
  blit goes on the sheet (as the map's later blits already do). `bo mapblit on|off`, panel checkbox, default
  on, status counter. The recorder now runs normal 10 s / MAPBLIT off 5 s / normal 5 s. BUILD-OFFLINE 3/3,
  DEPLOYED 04009A8E...FA215E. HEADSET + recorder pending. Same fix ported to City (BATMAN_CITY_PLAN "MAPBLIT").

## MAPFLAT - why four map fixes failed, and the structural fix (2026-10-11 evening; JJ: City + Origins "same issues still")
**Looked up again** (two+ failed fixes, SESSION_START rule; new words "full-screen UI state", "everything else once"):
playbook @ b955f224 `docs/symptom-index.md` l.342 "HUD or menus drawn twice, or doubled in one eye" -> `17` #side-effect-gate
("render the world twice, everything else EXACTLY once" + the third class "overwritten between write and read"), l.379
"full-screen UI misclassified" -> `04` "Find the engine's authoritative UI state instead of inferring it" (proxies fail on
different content types; find the game-state field); City scene.cpp MENUCLOCK (clock signal, lags up to 7 s, City only).
**What we missed (LIVE, batman_origins.prev.log 17:51-18:15, MAPBLIT build):**
1. MAPBLIT never acted on the real map. Map open ~18:07:29-18:07:54: "second scene blits" 20 -> 153, EARLYSTRICT kept 7
   with reason "second scene blit seen recently" (never "map screen up"), MAPBLIT stuck at 162 the whole time.
2. The MAPSCREEN detector (untextured shape >= 2000 indices) is the wrong signal: its only firing (17:51:35, 2103 indices)
   was the menu -> game bat transition, and City's log fires on the same 2103-index shape at its main menu -> game wipe.
   Origins' map draws no such shape. Every map fix keyed on it (MAPBLIT, EARLYSTRICT "map screen up") was off in the map.
3. Origins never tells the shared HUD code that a menu is up: `set_hud_zones_gameplay` / `set_hud_menu_now` are never
   called from the Origins adapter (grep), so `g_zonesGameplay` stays true = every menu runs the GAMEPLAY rules.
4. Root cause of the leak itself (MAPTRACE 17:37): the doubled viewport draw runs the map's own drawing per eye; one pass
   leaves the map art in the shared scene picture and the other eye's scene blit puts it full-view in the eye. The map has
   no 3D world, so it should not be rendered twice at all (playbook 17: everything except the world exactly once).
**Plan MAPFLAT (both games, port by code shape):** (a) an authoritative "full-screen menu/map is up" state from the game:
log Scaleform movie open/close (UE3 GFxMoviePlayer Start/Close or the game's own menu manager) with names, pick the map /
database / upgrade / pause entries; (b) while it is up: no second render (no doubling, plain flat camera), the WHOLE
backbuffer frame goes to the menu screen in one pass, eyes black; (c) Origins adapter sets zones-gameplay / menu-now from
the same state. Interim fallback switch: drive MAPBLIT from "second scene blit seen" (rose only in the map) as well.
Verify with the existing recorder (MAPTRACE): leaking eye pictures 0 in the map; control step (MAPFLAT off) > 0; JJ sees no
copy; pause menu, database, upgrade screen, loading and gameplay HUD unchanged. HEADSET: pending.
### MAPFLAT step 1: MENUSTATE probe (2026-10-11 ~22:40)
SOURCE: `src/game/origins/menu_state.cpp`, port of City's: the game's menu natives found BY NAME in the live native
table (the Origins exe holds no "<Class>exec<Func>" strings on disk - offline extraction found 0 while the same script
reproduces City's 2630; they appear only once the game runs), hooked on a worker thread, every call logged with the
movie's class/object name (GNames found live). Status line per 5 s; `bo menuprobe`. No behaviour change.
STATIC: Release built clean, CTests 3/3, installed + hash-verified (4059C89A6B47F9E9..., receipt updated), backup
re/backups/before-menustate. LIVE/HEADSET pending.
| # | JJ does | judge |
|---|---|---|
| 0 | launch, into gameplay | log "N of 16 menu natives hooked" (0 = Origins class names differ: list them from the strings) |
| 1 | pause / unpause; map open / close; database open / close | MENUSTATE lines name each screen's open and close |
Next: MAPFLAT keyed on that signal (no doubled render + whole frame on the menu screen while the map is up).
MS2 (same evening): menu_state ported again from City's MS2 (menu open / closed tracking logged, name table page scan +
UTF-16 + retry). Installed, CTests 3/3, receipt updated (C320E2885F6E426C...). City MS1: the probe found the pause / map /
upgrades signals (BATMAN_CITY_PLAN "MS1 RESULTS"); Origins' own run still pending.
### MS3: MAPBLIT on the game's own menu signal (2026-10-11 ~23:2x)
City result (HEADSET, JJ, runs 22:39 + 22:51): the copy outside the map / upgrades window is gone. City's fix = MAPBLIT
(on the map the eye is cleared black and the scene blit goes on the menu sheet; MAPTRACE 17:37 cause: the map art left in
the shared scene picture by the other eye's pass) + MAPBLITTAIL. Origins already had MAPBLIT, but its trigger never fired
on Origins' map (above). MENUSTATE names (City 22:51): pause = RGFxMovieUI_Pause, map / upgrades = RGFxMovieBackScreen.
SOURCE: menu_state tracks each open menu's class; "screen menu" = open and not pause-type (not "Pause" in the class, no
RestoreMenuState); HUD classes (Hud / FrontMost) never count. gfx_hud MAPBLIT source 1 (default) = that signal (set per
CalcSceneView by the adapter), 0 = the old shape detector (`bo mapblit src 0|1`); EARLYSTRICT "map type" also on it;
`bo menureset` clears a stuck state. STATIC: built clean, CTests 3/3, installed + receipt (E3AC869DD1942B19...), backup
re/backups/before-menustate/batman_origins.ms2.dll. LIVE/HEADSET pending.
| # | JJ does | judge |
|---|---|---|
| 1 | gameplay 10 s | log: "N of 16 natives hooked", name table found; no "menu signal ON" in gameplay |
| 2 | open the map, look at the edges 15 s; panel "Map flicker recorder" while it is up | no copy; log "menu OPEN <class>" + "MAPBLIT menu signal ON"; recorder: leaks 0 in normal steps, > 0 with MAPBLIT off |
| 3 | close the map | signal off at once; no 2D gameplay picture on the menu screen |
| 4 | database, pause menu | database clean; pause unchanged (pause is not a screen menu) |
- MS3 install fix (2026-10-11 ~23:3x): the launcher checks `<Win32>\originsvr_manifest.json` (not re/evidence) - the MS1-MS3 installs had not updated it, so Launch-VR refused ("Installed file differs"). Updated to E3AC869D...; every manifest file re-checked OK; backup re/backups/before-menustate/originsvr_manifest.json.
### MS3 RESULTS (23:17-23:18, JJ HEADSET: "still getting the flickering outside the map screen") -> MS4
- LIVE: name table found (5965 names, ANSI +0xC) but "0 of 16 menu natives hooked": every "<Class>exec<Func>" search
  found 0 strings. Origins has NO name-based native table (not on disk, not live) - City does. So the menu signal never
  came and MAPBLIT never switched on: the run was the old state. Lesson: a build whose fix depends on a signal must say
  loudly when the signal is missing (now logged: "NO menu functions hooked - the menu signal is OFF").
- SOURCE MS4 (menu_state, both games; Origins uses it, City keeps its table route): the UFunction route - GObjects found
  live (the TArray whose objects resolve Name / Class / Class->Class = "Class"); the function objects named Start / Close
  / SetFocus ... whose owner (the pointer before Name, checked against "GFxMoviePlayer") is the class; Func = the offset
  where >= 95 % of 1500 Function objects hold a code pointer and >= 30 % share one value (script ProcessInternal); the
  hook swaps that one function object's Func (data write, no code patch). Retried every 3 s for 2 min.
- STATIC: built clean, CTests 3/3, installed; game-folder manifest + receipt updated (6ABFEEE233967249...), launcher check
  re-run: passes. Backup re/backups/before-menustate/batman_origins.ms3.dll. HEADSET pending.
- Judge: log "object list at", "function code pointer at +0x..", "N menu functions hooked through their function
  objects", then "menu OPEN <class>" + "MAPBLIT menu signal ON" when the map opens; JJ: no copy at the edges.
### MS4 RESULTS (23:22-23:24, JJ: map still flickers; JJ quit normally, WM_CLOSE, no fault) -> MS5
- LIVE: object list found (137282 objects, name +0x1C, owner +0x18); "code pointer" taken at +0xA0 because the code
  range was the whole image: 013CC348 (shared by 1399/1500) is in .rdata, not .text. 14 of 16 then "not native"; two
  were "hooked" = our detour written into a NON-code field of RPlayerController.GetActiveFocusMovie / SetRegularGame
  PauseFlag (+0xA0 held 5C801C0B / 1F1CB460; the second was rewritten to 0 by the game). Live repair: the first field
  restored (ReadProcessMemory showed 59263AF0 = our DLL) - the process had already exited. No crash. Rolled the install
  back to MS3 (no hooks) at once.
- SOURCE MS5: route A (preferred) = short per-class native tables { "execStart", impl } - a run of "exec*" entries is
  used for a class only if it holds all of the class's anchors (GFxMoviePlayer: Start, Close, SetFocus; ...) and is the
  only such run; code hooks + shared-stub check as City. Route B (fallback) = function objects with the real .text
  range (script value and target values must be in .text). Read-only evidence: which words of GFxMoviePlayer.Start
  point into code. STATIC: built clean, CTests 3/3, installed D27F868742F925CB..., manifest + receipt, launcher check
  passes. HEADSET pending.
### MS5 RESULTS (23:28-23:29, JJ: still flickers) -> MS6
- LIVE: no short "exec<Func>" tables either (all four groups "not found"). Function objects found (owner +0x18);
  GFxMoviePlayer.Start object code words: +0x88 = 00BB2260, +0x128 = 00BB2340. STATIC (capstone, installed exe):
  0xBB2260 = execStart thunk (reads the optional bool via GNatives 0x1812C10, then calls on); 0xBB2340 = the same shape
  with a float = the NEXT function object's +0x88 (objects 0xA0 apart). => Func at +0x88. The 95 % rule never matched.
- SOURCE MS6: Func offset = the single offset where Start, Close and SetFocus all hold distinct .text addresses (else
  no hooks, logged); pointer swap per function object. Built clean, CTests 3/3, installed 9A7B46E615C212D9..., manifest
  + receipt, launcher check passes. Expected log: "function code pointer at +0x88 (Start / Close / SetFocus: 00BB2260 ...)".
### MS6 RESULTS (23:32-23:35, JJ HEADSET + LIVE) -> MS7
- LIVE: Func at +0x88 confirmed (Start / Close / SetFocus 00BB2260 00BB2430 00BB2BB0); 16/16 hooked through function
  objects. Names: pause = RGFxMovieUI_Pause; the map = MGFxMovieControllerBackScreen + ...BackScreenTop + ...Map, opened
  within 20 ms. MAPBLIT menu signal ON in the map, MAPBLIT count rose every pass.
- HEADSET (JJ): copy still at the edges, "follows your face as your head moves". After the map: flat 2D (BUG: Origins
  Closes Map + BackScreen but never BackScreenTop -> the screen-menu state stayed on -> MAPBLIT kept the eyes black and
  the game on the menu sheet). Repaired live with `bo menureset` (4 menus dropped).
- LIVE: EARLYUI "passes without the scene blit" rose ~2.7/s in the map (16 -> 70) - MAPBLIT acts only at the scene blit,
  so those passes keep the eye's old content (head-locked, flickering) = suspect. The MAPTRACE run (23:34:36-56) ran while
  JJ was in the PAUSE menu, not the map: its leak counts (0 / 788 / 1) do not answer the map question.
- SOURCE MS7: menus opened within 300 ms close together; MAPBLIT2 = on a screen menu, a pass without the scene blit is
  blanked too (counter in the 5 s MENUSTATE line). Built clean, CTests 3/3, installed 466E04DC3DFF3FCF..., manifest +
  receipt, launcher check passes. Test: map + recorder IN the map; after closing, 3D (log "closed with it").
### MS7 RESULTS (23:38-23:40, JJ HEADSET: "you've made it worse") -> MS8 = undo
- HEADSET: copy still outside the window; the map "attached to the camera"; booting flips 2D / 3D / 2D / 3D.
- LIVE: (1) the title chain (RGFxMovieUI_PressStart, Login, DLCScanning, StorageSelect, SaveSlotSelect, MainMenu) counted
  as screen menus -> MAPBLIT signal ON / off while booting = the flips. (2) MAPBLIT2 blanked only 17 passes in 90 s of map
  - not the copy. (3) ROOT MISREAD: Origins' map is a real 3D hologram rendered by the engine into each eye; MAPBLIT moved
  it onto the flat sheet = "attached to the camera". The MAPTRACE leak check counted that legitimate 3D map in the eye as
  a "leak" (176 / 788 leaks) - the instrument was wrong for Origins, and MAPBLIT is wrong for Origins.
- SOURCE MS8: MAPBLIT default OFF in Origins (test only); screen-menu signal only for map-type classes ("BackScreen" /
  "Map"); the probe and group-close stay (logging only). Built clean, CTests 3/3, installed 2C420E5DC677AF31...,
  manifest + receipt, launcher check passes. Expected: boot and map as before tonight (copy still there).
- The Origins copy is still UNEXPLAINED. Facts: head-locked, at the edges outside the map window, flickers ~1/s; the Quest
  video frame (~10 s, temp/Origins_Map_Flicker.mp4) showed a cyan/red full-view grid left of the window. Next: an
  objective per-eye picture burst in the map (consecutive eye pictures + HUD sheet) to find the frames that hold the copy,
  then name their draws - before any further fix.

## TEST PLAN EB1 - black window before the first menu + window-sized bats -> full view (EYEBATS / BOOTBLACK), 2026-10-13
JJ: "a black window that appears after the title screen video and before the first menu screen ... needs to be removed";
"Arkham City had the same issue. Same with the bat wipe transition screen. Needs to be enlarged to full FOV and brought
closer to the camera".
Looked up: VR_ESSENTIALS §6 (VD crops/smears lone quads; menus/fades into the eyes) and §2 City recipes; City
BATMAN_CITY_PLAN WIPEREPLAY / WIPEFLIP / WIPEPANEL / BLACKFILL / MENUFADE (in-eye replay over the whole view, panel not
sent while it plays; the game-start stage fill -> whole view black, JJ "perfect"); memory batman-city-hud-overlays;
this plan O1F-O1H (head-relative quad layer, JJ O1G "window-sized"). Playbook b955f224 HUD-004 already cited in O1F.
- LIVE (O1H log 23:47): logo skip 33.75 -> cinematic quad off 34.17 -> UI frames of only 1-2 UNTEXTURED draws until the
  title's first textured UI at 35.64 (event_1 hud_last_frames) = Origins' full-screen black fill, shown on the floating
  panel = the black window. Origins turned City's BLACKFILL off (scene.cpp: City classifiers not Origins evidence).
- Cause of the window-sized bats: they were a separate head-relative quad layer; VD shows it as a window.
- SOURCE fix: EYEBATS - the runtime calls the adapter's painter on every FINISHED eye image (after DLSS / held-left
  finish / AFW warp / room screen); the bat recording is drawn over the whole eye (eye_enlargement 1.6 = City's in-eye
  size), shifted per eye so both eyes fuse it on a plane straight ahead at the bat distance (0.8 m), correct for the
  asymmetric lens. One playback step per XR frame (both eyes same picture). Quad layer no longer submitted. While the
  eyes hold bats/black the floating UI panel is not sent (City WIPEPANEL); 1.5 s after, only its coloured UI.
  BOOTBLACK - until the first menu (title-exit bat event, or 60 textured UI frames in the 3D view), a UI frame made of
  ONLY untextured draws (3D view only, never during movies) makes the whole view black for 150 ms.
- STATIC: built clean; CTests 3/3 incl. new WARP checks (eye shift moves the bats exactly; BOOTBLACK fills the whole eye;
  a textured menu frame is not blackened). Installed batman_origins.dll 33D79327..., manifest + receipt updated, all 16
  files verified. Backups: re/backups/before-eyebats (sources, installed DLL, manifest).

| # | JJ does | judge |
|---|---|---|
| 1 | launch VR, skip the logos | no black window: whole view black, then the title screen; log "BOOTBLACK: ... whole view black", "BOOTBLACK done" |
| 2 | press Start | bats reveal the title scene over the WHOLE view, no window; auto folder bat_layers_*_event_1 with eye_left/right.bmp |
| 3 | main menu -> Continue | bats cover the whole view -> black over loading -> reveal; no panel window during it |
| 4 | panel BAT TRANSITIONS: Bat distance / enlargement if wanted | both sliders live; eyes fuse the bats |
| 5 | pause -> quit to menu | same full-view bats; HUD returns after |
Follow-ups: bats look doubled -> distance slider (shift per eye); edges of the game's own bats show on the panel after
the replay -> extend the 1.5 s coloured-only window; black window still there -> `bo bats status` + capture during it.
- 2026-10-13 City TABRACE checked for Origins (JJ asked): NOT applicable. City's black Tab screen came from the HUD's
  "gameplay" flag flipping to "menus" when the game clock stopped ~40 ms before the opening wipe. Origins never sets that
  flag (no set_hud_zones_gameplay caller; g_zonesGameplay stays true), so its openings always count as from gameplay and
  MENUFADE / in-menu holds never run. The Origins map copy is a separate, still-unexplained problem (above).
- 2026-10-13 PANELMOUSE ported from City (JJ: City's overlay mouse works): overlay.cpp maps the QUEUED mouse events onto the
  panel (no io.MousePos scaling), recursive lock around ImGui's input queue, dinput_block stills the game's DirectInput
  mouse while the panel is open. Release built clean, CTests 3/3, installed via tools/Install-VR.ps1 (manifest updated),
  batman_origins.dll 5B5DE7190613860C..., backup re/backups/before-panelmouse. Not run.
- HEADSET (JJ 2026-10-13): PANELMOUSE "looks good in Origins".

## TEST PLAN EYB1 - the map flicker, recorded objectively (2026-10-13; JJ: "the only annoying thing now with origins is the
## flickering duplicate in the map/tab menu. We solved it for city, but it's persisting in origins")
Looked up (again; 7 failed fixes on 2026-10-11): this plan MT1 / MT2 / MAPTRACE / MAPFLAT / MS3-MS8; memory
arkham-map-flicker-wrong-signal (City's fix MAPBLIT is WRONG for Origins: its map is a 3D hologram drawn into each eye);
VR_ESSENTIALS §2 (anything cached on game time must refresh per eye and while paused - PERCAPTURE / PAUSEREFLECT; Origins'
reflect.cpp hooks only the reflection probe class, RE_NOTES "Reflection probe", so it says nothing about the map); City
TABRACE (does not apply: Origins never sets the gameplay flag). Video temp/Origins_Map_Flicker.mp4 re-read frame by frame
(scratchpad): flash frames differ from both neighbours while the neighbours match; no clean shift of the window, the text
looks doubled - one eye's video at 30 fps under compression cannot name the cause.
LIVE (today 17:02, 1.5 s in the map): presents 891 = pairs 446 x 2, root = second = 445 per 5 s -> every map present is a
tagged eye present with both eyes rendered; the flicker is INSIDE the eye pictures (not untagged presents).
Ranked causes: (1) the other eye's map art reaches this eye (a shared picture written in one eye's pass, read in the
next - MAPTRACE 17:37 saw map art in eye-1 scene blits); (2) an off-screen picture of the map refreshed on game time /
frame number (clock stopped in the map), drawn from one eye's view and used by both; (3) a pass whose picture is not
this eye's at all (a stale / skipped pass).
SOURCE EYEBURST (core vr/eye_burst.cpp, wired at the eye present before anything is painted in): `bo eyeburst N`
records N presents - a 1/4-size copy of the window picture (= what goes into that eye) and of the HUD sheet (= the menu
layer) per present, plus index.txt (seq, time, eye sign, pose generation, AFW, EARLYUI / second-blit / MAPBLIT
counters). Staging ring of 4, read 4 presents later (no GPU stalls). Files captures\eyeburst_<time>\.
tools\eyeburst_view.py <folder>: per eye, a flash = differs from that eye's previous AND next picture while those match;
flashes.txt ranked + flash_<seq>_<eye|hud>.png (before / flash / after / the other eye nearest in time; HUD sheet below).
Tested on synthetic data (planted flash found, score 59). STATIC: built clean, CTests 3/3, installed via Install-VR.ps1
0C485C49BD92FFE9..., backup re/backups/before-eyeburst.
| # | JJ does | I send | what it proves | how to judge |
|---|---|---|---|---|
| 0 | launch VR, load into gameplay, "ready" | - | install | log "[bo]" status lines |
| 1 | stand still in gameplay 5 s | `bo eyeburst 360` | control | flashes.txt: top scores small (< 2), no flash pictures |
| 2 | open the map, look at the window edges, keep still, say "in map" | `bo eyeburst 540` | the flicker in the record | flashes > 2 in eye or hud; JJ: saw flickers during the 3 s |
| 3 | stay in the map | `bo stereo off`, `bo eyeburst 360`, `bo stereo on` | the doubled render is the route | stereo off: no flashes -> cause (1)/(3) |
| 4 | stay in the map | `bo afw on`, `bo eyeburst 360`, `bo afw off` | one eye drawn per frame | AFW: no flashes -> the second render's side effect |
| 5 | upgrades tab of the same screen | `bo eyeburst 360` | 3D map vs flat tab | flashes only with the 3D map -> the map render |
Follow-ups: flash picture = the other eye's map (matches the 4th column) -> cause (1): find the shared picture (frame
dump of one flash pass); flash = same eye's older map -> cause (2)/(3): read the pose generation column (repeat / jump);
flash only on the HUD sheet -> the menu layer, not the eyes.
### EYB1 reworked as ONE PANEL BUTTON (JJ: "I can't work interactively with you while I'm in the headset" - VR_ESSENTIALS §1)
- SOURCE MAPTEST2: panel HUD -> "Map flicker test (30 s, open the map first)" (replaces the old MAPTRACE button; `bo
  maptest2` too). 4 s to close the panel, then 4 steps, each: switch, 1.5 s settle, EYEBURST record (540 / 360 / 360 /
  360 presents), log "[bo] MAPTEST2 step ...": 1 normal, 2 stereo off (one picture for both eyes), 3 AFW, 4 normal again;
  then restores native + AFW as they were. Gameplay control dropped (steps 1 / 4 vs 2 / 3 compare within the map).
- STATIC: built clean, CTests 3/3, installed 32FFF0A10D0647E3... (backup re/backups/before-eyeburst = the PANELMOUSE build).
- JJ: start Origins in VR, open the map, open the panel, HUD drawer, press the button, close the panel, keep still ~30 s
  looking at the window edges; optionally press it again on the upgrades tab. Then I read the 4-8 records offline.
### EYB1 RESULTS (17:33, JJ pressed MAPTEST2 in the map; LIVE + objective pictures) -> MAPSAFE
- Records eyeburst_173310 / 173316 / 173322 / 173328 (normal 540, stereo off 360, AFW 360, normal 360 presents).
- OBJECTIVE: in the map EVERY eye picture is pure black (lit pixels median 0) and the whole map is on the HUD sheet (~104k
  lit px at 1/4 size) - EXCEPT single passes: normal L 44 + L 106 (2 in 3 s), AFW R 271, normal again L 114; stereo off
  (one picture for both eyes) 0. In each flash pass the WHOLE map (103.7k lit px) is in the eye picture at screen pixels
  and the sheet holds only the text (10.1k) = the flickering head-locked duplicate. Neighbours of each flash are black.
- No logged counter moves at the flash passes (EARLYUI "caught" rises at other presents; second scene blits constant 48
  in the map) -> those passes never started the UI run before the map pieces: the pieces went down the pre-boundary
  route straight into the eye; the text came after the run started. (MS7's "3D hologram drawn into each eye" is NOT what
  the pictures show here: the eyes are black, the map is on the sheet.)
- SOURCE MAPSAFE (gfx_hud classify): while the game's own signal says a map-type screen is open (MENUSTATE BackScreen /
  Map -> set_menu_screen) and the HUD redirect is on, the first backbuffer draw before the boundary starts the UI run at
  once (no eye-pass flag needed) and clears the eye black; a full-screen blit after such a start is dropped (EARLYSTRICT
  would have put that opaque blit ON the sheet). Counters: starts / starts without the eye-pass flag / blits dropped,
  logged by MAPTEST2 and recorded per present in EYEBURST index.txt. `bo mapsafe on|off`, panel HUD checkbox, default ON.
- MAPTEST2 now: MAPSAFE on 6 s / off 6 s / on 6 s (1080 presents each), restores. STATIC: built clean, CTests 3/3,
  installed 4FCEB2F6EF1218DF... Not run.
- Judge: flashes 0 in steps 1 + 3, > 0 in step 2; "starts" rises in steps 1/3 (and "without eye pass" says why EARLYUI
  missed); JJ sees no flicker at the map edges; after closing the map, gameplay picture normal (risk: a menu signal stuck
  on would make MAPSAFE drop the scene blit in gameplay = black eyes; MS7 group close should prevent it).
### MAPSAFE run 1 (17:45, JJ HEADSET: "When I first entered the map it didn't flicker ... then all of a sudden it flickered
### and then the map became attached to my face and now I can't exit out of that mode. The map just stays there when I exit")
- LIVE: MAPTEST2 was NOT pressed (MAPSAFE simply on by default). Map open 17:45:11.65-19.42 (7.7 s): MAPSAFE dropped a
  full-screen blit at 16.29 and 17.20 (log capped at 3 lines; the start counters were not logged anywhere). The map signal
  went OFF at 19.446 as it should (and on/off again 20.65-21.49), so MAPSAFE was inactive when the stuck map was seen in
  gameplay -> the stuck picture is a LEFTOVER state, not MAPSAFE acting. Pause 26.44, quit 28.60 (known exit fault).
- Not yet known what holds the map after closing (candidates: the HUD sheet no longer cleared / redirected; a picture
  held by the DLSS / eye path; the dropped blit leaving the backbuffer content in use). No guess fixes.
- SOURCE: MAPSAFE default OFF (test switch only). MAPREC (scene.cpp, default on in this test build): every map opening
  records 8 s and every closing 5 s with EYEBURST, no button; MAPSAFE counters now in the 5 s status line and in each
  MAPREC line. `bo maprec on|off`. STATIC: built clean, CTests 3/3, installed 7468A8D709581B6B...
| # | JJ does (no talking) | records | judge |
|---|---|---|---|
| 1 | gameplay, open the map ~8 s, close, play ~5 s | open + close, MAPSAFE off | baseline flashes; after-close pictures normal |
| 2 | VR menu > HUD > tick MAPSAFE; open the map ~8 s, close, play ~5 s; repeat once | 2 x (open + close), MAPSAFE on | flashes gone?; if the map sticks: the close record shows WHERE (eye or sheet) and from which present |
| 3 | if stuck: VR menu > HUD > untick MAPSAFE, look 5 s, then quit; tell me afterwards whether unticking cleared it | - | clears = an ongoing MAPSAFE effect; stays = a leftover |
### Run 17:51 (JJ ticked "Map: eyes black, map art on the sheet (MAPBLIT)" by mistake -> "everything has frozen and become
### stuck to my face")
- LIVE: MAPSAFE stayed OFF the whole run (starts 0); "MAPBLIT2 passes blanked" rose from 17:51:45 = MAPBLIT ticked (MS7's
  known-bad face-locked map). Fixed remotely: `bo mapsafe off` 17:52:11 (no effect, was off), `bo mapblit off` 17:52:30.
  So the 17:45 "stuck to my face" (MAPBLIT 0 then) WAS MAPSAFE; this one was MAPBLIT.
- MAPREC baseline eyeburst_175126 (map open, both fixes off, 1200 presents / 6.7 s): 5 flashes, ALL LEFT eye (612, 832, 914,
  1008, 1190) - the first pass of each pair. Same signature as EYB1 (left in normal steps).
- PANELWORDS (JJ: "Some of the labels on these things are very confusing"): HUD drawer = only player settings in plain
  words; MAPBLIT removed from the menu (command only); EARLYUI / EARLYSTRICT / menu flicker test -> commands only; new
  drawer "TESTS (only when Claude asks)" with the MAPSAFE box labelled "EXPERIMENTAL, once made the map stick to your face"
  and "Record the map for 30 s". Built clean, CTests 3/3, installed 257CA84E371DAE79...
- Open: why MAPSAFE (17:45) left the map stuck after closing it - the next step reads its MAPREC close record; first build
  a sim / offline check before JJ touches it again.

## TEST PLAN EYB3 - which routing rule sends the map into the LEFT eye with DLSS on (2026-10-13 ~18:45)
JJ: "tested with DLSS disabled and the flickering wasn't as bad. It started a bit later after entering the map, but it still
happened." OBJECTIVE (MAPREC records): DLSS OFF run 18:39 (log .prev): map visits eyeburst_183902 + 183927 = 0 flash pictures
in 2 x 6.7 s (183955: 1 untagged); DLSS ON run 18:41 eyeburst_184140 = 5 flashes in 6.7 s, ALL LEFT eye (360, 434, 462, 554,
766) - the eye OVERLAP holds back while DLSS finishes it. Earlier DLSS-on records: all left too (EYB1 normal steps, 175126).
Looked up: DLSS only copies at present (dlss_bridge 477/518/707/740/761: backbuffer -> DLSS colour, output -> swapchain), it
draws nothing inside the game's pass -> it changes TIMING, not routing. The flash passes are TAGGED left (index sign -1), so
BeginDrawing found the tag: redirect on + eye pass open (scene.cpp 159-160). Remaining route candidates inside classify:
(1) the transition "keep in eye" rule (cut pieces / pure-cut frames / wipeRest - City OPENFADE saw a map-opening frame sent
full-view into the eye by the pure-cut rule); the map's floor plan = untextured shapes, text = textured -> matches "map in
the eye, only text on the sheet"; (2) the pre-boundary route without EARLYUI; (3) something DLSS timing changes per pass.
SOURCE EYB3: tracer letters for the keep rule (C cut piece, W after a wipe frame, G after a pure-cut frame); MAPREC now also
starts LEAKTRACE (captures\maptrace_<time>\leaktrace.txt: one line per present, eye-bound draws "pre:"/"eye:" with count +
texture size, routing letters, leaking eye pictures saved) for every map opening; EYEBURST2 adds the FINISHED headset images
(after DLSS / held-left finish / bat paint: final.raw + final.txt), 1/8 size, open 10 s / close 5 s; index states: redirect,
HUD menu mode, HUD head-locked. viewer prints per-stream lit pixels + saves 3 final pictures. MAPSAFE stays OFF.
STATIC: built clean, CTests 3/3, installed E4D9066C2485BF43...
| # | JJ does (DLSS ON, no talking) | proves | judge |
|---|---|---|---|
| 1 | load into gameplay, open the map, keep still ~10 s, close, play ~5 s | the route | leaktrace: leaking presents (left) list "eye:" draws (post-boundary, + C/W/G = keep rule) or "pre:" (pre-boundary); final images show how many frames DLSS carries each flash |
| 2 | repeat once (more flashes) | repeatability | same letters on every leak |
Prepared: keep rule -> on a map-type screen (MENUSTATE) no piece is kept in the eye (cut rules off while the map is open,
nothing else changes); pre-boundary -> EARLYUI without the eye-pass flag on map screens only (narrow, not MAPSAFE's blit drop).
### EYB3 RESULTS (18:54-18:55, JJ DLSS on, 3 map visits; LIVE traces maptrace_185451 / 185510 / 185525) -> MAPHOLD + VIEWDEBOUNCE
- OBJECTIVE: every map leak is ONE pass whose map pieces never reached our sorting: a LEFT pass with "draws 38 sheet 37"
  (normal 76-78; p4676, p6515, p6853, p7287, p9027 ...) - the ~40 map pieces went into the game's scene picture before the
  scene blit (not the backbuffer, so never counted), which then carried them full-view into the eye; plus one UNTAGGED pass
  (p4890 "sign 0 redirect 0 ... sheet 0 eye 77": no eye tag -> menu layer off -> everything into the eye; untagged
  presents go to the LEFT swapchain = why always left). Smaller ones (39-45 pieces) leak partly (below the leak threshold).
  The keep rule (C/W/G letters) never fired: ruled out. DLSS changes the pass timing that makes these passes likelier.
- JJ: "the very last one during the flickering, it actually reset the centering". LIVE 18:55:40.545 / 41.044: "camera mode
  off" -> "ON" within 20 ms, twice: scene.cpp turned camera mode off for any draw root with no 3D view (one map frame).
- SOURCE MAPHOLD (gfx_hud before_eye_capture, after the tracer): on a map-type screen (MENUSTATE), a pass with < 60 % of
  the last full pass's sheet pieces, or no redirect, gets its eye picture cleared black and the last full map sheet put
  back (max 4 in a row; settled 0.5 s after opening). Offline replay of the 3 traces: every leak pass held; the other holds
  are the same kind of left pass (39-45 pieces). `bo maphold on|off`, TESTS drawer box, status line, default ON.
- SOURCE VIEWDEBOUNCE (scene.cpp): camera mode off only after 300 ms without a 3D view. `bo viewdebounce 0|1`, default ON.
- STATIC: built clean, CTests 3/3, installed 9A0DA7E9BFA4A7E0... MAPREC + tracer still record each map visit.
| # | JJ does (DLSS on, no talking) | judge |
|---|---|---|
| 1 | gameplay, open the map ~15 s, look around, close, play a bit; repeat twice; switch map / upgrades tabs once | JJ: no flicker, no re-centre, map moves normally; log "MAPHOLD pass with N of M map pieces" lines; status "map passes replaced" rises in the map only; MAPREC final images: no map in the eyes |
| 2 | if anything is wrong: VR menu > TESTS > untick "Map flicker fix 2" | the old behaviour returns (A/B) |
### MAPHOLD run 1 (19:03, JJ HEADSET: "fine for about 15 to 20 seconds and then the map flicker outside the map returned and it
### reset the centering") -> MAPHOLD2 + VIEWDEBOUNCE2
- LIVE: MAPHOLD held passes (38/41/38/44/37/38 of 75-77 ...), counts +3 / +13 / +26 / +42 / +33 per 5 s. Trace 190324: short
  passes carry 37-59 pieces (all LEFT); the 60 % rule let 50-59 through AND each became the new reference (g_mapFull = last
  pass), after which 38-piece passes were >= 60 % of it = no longer held -> the flicker returned. Glitch RATE unchanged around
  the event (13 / 954 presents before, 10 / 684 after).
- Re-centre: 19:03:32.384 camera mode off -> ON in 18 ms with VIEWDEBOUNCE on: the OTHER path - a draw root from another
  caller (ra != gameplay caller) makes eligible false -> set_camera_mode(false). The same frame = the untagged pass p4455.
- SOURCE MAPHOLD2: reference = fullest recent pass (only raised), pass < 90 % of it held; a real screen change accepted after
  8 short passes in a row; 8 passes without the menu layer stop holding (no stuck state). VIEWDEBOUNCE2: a foreign root leaves
  camera mode alone while the gameplay caller drew within 300 ms. Offline replay of all 4 traces: every short pass held
  (19 / 25 / 14 / 30), no reference drift, no false screen changes.
- STATIC: built clean, CTests 3/3, installed BB749941DABB2997... HEADSET pending. (Steam busy with an AKN xrsim run by
  vr-modding-0a for ~40-60 min from ~19:10.)
### MAPHOLD2 run (21:29-21:31, JJ HEADSET: "a lot better, but the flickering is still there a little bit off to the left and I
### get the occasional full field of view blackout for one frame and also occasionally recenters ... not a deal breaker") -> MAPMONO
- OBJECTIVE: finished headset images in both 10 s map records (eyeburst_213005 / 213042, 1798 each): eye lit max 114 = no map
  in the eyes. Near-empty sheets only at the map opening (fade). MAPHOLD replaced ~14 passes/s. The leftover events fall
  OUTSIDE the 10 s records: log "MAPHOLD menu layer off for 8 passes - hold stopped" at 21:30:32 and 21:31:04 = runs of
  untagged passes (tag-miss 0 the whole run, no camera-mode toggles = the game's own mono path: !ordinary / not active).
  Untagged presents in projection mode are captured into the LEFT swapchain with a stale pose = flicker on the left, a black
  frame (MAPHOLD clears the eye), a jump (re-centre feel).
- SOURCE MAPMONO (openxr_runtime on_present_end): while the map signal is on (gfx_hud::menu_screen_on), an untagged present
  in projection mode is not captured - the headset keeps the last stereo pair. `bo mapmono 0|1`, default on; status
  "mono map presents kept from the headset N". EYB4: the tracer now covers the whole map visit (10800 presents).
- STATIC: built clean, CTests 3/3, installed E761E913D6EE9CB7... HEADSET pending.
### MAPMONO run (21:39-21:41, JJ HEADSET: "those same issues are still present") -> MAPHOLD3 + VIEWDEBOUNCE3
- OBJECTIVE (trace maptrace_214005, whole visit 10123 presents; eyeburst_214005 first 10 s): eye pictures black in every
  recorded present. Runs of 20-24 passes with ONE sheet piece (the game's quick fade) at the opening (p3395) and twice more
  ~25 s apart (p7921, p12411): MAPHOLD held 8, then "hold stopped" let the empty sheet through for ~15 = full-view black.
  Two untagged passes (p3687, p4968; eye 75) = the two camera-mode off -> ON toggles (21:40:07.635, 16.426): the GAMEPLAY
  caller ineligible for one frame (no fresh pose generation), not a foreign caller - VIEWDEBOUNCE2 did not cover it; with
  camera mode off that frame is not in projection mode, so MAPMONO did not skip it either (= flicker + jump).
- SOURCE MAPHOLD3: a near-empty pass (< 15 % of the reference) is held up to 90 passes (~0.5 s), a real screen change still
  after 8. VIEWDEBOUNCE3: any single ineligible root (any caller) within 300 ms of an eligible one leaves camera mode alone
  while head tracking + native stereo are wanted -> the frame renders mono and MAPMONO keeps it off the headset on the map.
- STATIC: built clean, CTests 3/3, installed 1762711057E51992... HEADSET pending (Steam busy: vr-modding-0a AK run ~21:45).

## RELEASE 0.1.0 - first public package (2026-10-14; JJ: "is there anything left outstanding ... if not then package it up")
Looked up: PACKAGING_GUIDE.md (all sections; §12 tidy the panel first + test tools behind one switch, §13 ship the
recorded bat transition + launcher data, uninstaller deletes the data folder); City's 0.1.4 package (install/*.ps1,
tools/make_package.py, README) as the pattern; City VSYNCOFF (Origins d3d11_hook already forces interval 0 while a
session runs, line 158); memory panel-labels-plain-no-bad-switches; VR_ESSENTIALS §6 panel rule.
- Outstanding check: MAPHOLD3 + VIEWDEBOUNCE3 (last entry above, "HEADSET pending"). LIVE 2026-10-14 01:13-01:15 (JJ, DLSS 4):
  37 s on the map, MAPHOLD replaced 389 short passes, NO "hold stopped" lines, NO camera-mode off/on toggles (= no blackouts,
  no re-centres in the log), normal quit (WM_CLOSE). Treated as accepted for the package; JJ's own verdict not recorded.
- Removed automatic recorders: MAPREC (every map visit recorded 10 s + leak trace) default OFF; the bats' automatic
  bat_layers_* sample at the first two events removed; old MENUTEST / MAPTRACE / MAPTEST2 threads removed (MAPTRACE and
  MAPTEST2 toggled MAPBLIT / MAPSAFE, both known to stick the map to the face). Status heartbeat every 30 s (5 s with
  test options on). pacetrace.log kept (City ships the same).
- Panel = City layout: title + status line, Recenter / Save capture / Head tracking buttons, controller hint, STEREO
  RENDERING, VIEW, HUD (City's degree sliders, head-attach, pause/map screen), MENUS AND SCREENS (bats, movie screen, this
  menu), VR RESOLUTION, DLSS, Keyboard shortcuts, then the TESTS drawer at the bottom (test numbers switch, reflections
  A/B, map fix A/B, map / eye-picture recorders, bat test buttons, diagnostics). New saved keys: [hud] on_head,
  menu_distance, menu_width; [screens] distance, width, panel_width, panel_distance; [stereo] afw, afw_mode. Meta-button
  recentre (City's runtime_recenter_count) added.
- Package: install/ (Install-BatmanOriginsVR, Uninstall-BatmanOriginsVR, the two launchers), tools/make_package.py,
  dist/BatmanOriginsVR-ArkhamOrigins.zip. Installer: first-install backup of GFXSettings xml + BmEngine.ini, DoF / motion
  blur / V-sync off, JJ's settings.ini + bat-wipe.bin into %LOCALAPPDATA%\Batman_Origins (first install only), shortcuts.
  Uninstaller: removes every mod file, puts both settings files back from that backup, deletes the data folder + shortcuts.
### RELEASE 0.1.0 RESULTS (2026-10-14)
- STATIC: Release built clean, CTests 3/3. batman_origins.dll SHA-256 516E120DD7D057A8... Practice install (scratchpad: fake
  game + JJ's pre-mod baseline files, foreign xinput1_3.dll): install x2, uninstall from the game folder -> BmEngine.ini and
  GFX xml byte-exact, foreign proxy back, data + backup folders gone; all four .ps1 parse clean; manifest 13 files, 0 mismatches.
- INSTALLED on JJ's game with the package installer (backup seeded from baselines/first-install = his originals; his
  settings.ini kept; shortcuts on the desktop + Start menu). Backup of the previous install: re/backups/before-release.
- RELEASED: commit 97b9aba pushed; GitHub release v0.1.0 with dist/BatmanOriginsVR-ArkhamOrigins.zip (62.7 MB, sha256
  d85421b7ba87db1d..., downloaded back and matched); repo description updated.
- HEADSET pending: the new panel and the removed recorders have not been run in the game yet. JJ: open the VR menu, go
  through each drawer, change a HUD slider, restart, check it was kept.

## RELEASE 0.1.1 - Pimax fixes (PIMAX, 2026-10-17; JJ: "fix all the mods so they work on Pimax correctly, then deploy")

Looked up: Asylum Pimax player log + batman_asylum BUILD_PLAN "RELEASE 0.1.1"; City BATMAN_CITY_PLAN SVRFALLBACK /
SVRACTIVE / ELEVATED. Origins' DLSS helper already never blocks (dlss_bridge on_eye returns until helperReady), so the
Asylum first-frame stall does not apply here. Origins had NO 32-bit fallback: on a Pimax PC (64-bit runtime registered
for 32-bit apps) bo_xr_config32 failed and Launch-VR.ps1 refused to start the game.

Fixes (SOURCE, built, offline): SVRFALLBACK + ELEVATED log ported into core openxr_runtime.cpp init_instance;
bo_xr_config32 exits 4 when no 32-bit runtime can be loaded, Launch-VR.ps1 then uses SteamVR's 32-bit runtime for the
launch (Find-SteamVR32, active install first). README: one Pimax troubleshooting row.

### RELEASE 0.1.1 RESULTS (2026-10-17)
- OFFLINE: bo_xr_config32 with VD's 64-bit manifest forced -> exit 4; launcher parses. Package changed vs 0.1.0 =
  batman_origins.dll, bo_xr_config32.exe, Launch-VR.ps1, version.txt, README; settings = the 0.1.0 zip's settings.ini.
- HEADSET pending (JJ regression check on VD; Pimax players).

## RELEASE 0.1.2 (same fix as Asylum) - Pimax menu / HUD double vision (PIMAX2, 2026-10-17)

Report (via JJ): Pimax player, "the main menu and HUD have double vision with OpenXR" (not with SteamVR). Looked up: VR_ESSENTIALS
§8 (VD quad layers have no parallax), memory vd-quad-layers-no-parallax, playbook b955f224 09 "Canted displays"; code: every
runtime-specific branch in openxr_runtime.cpp. The HUD in-eye overlay and the distant menu screen project through each eye's own
pose (orientation + position) - correct on any runtime, canted or not. The ONE per-eye geometry branch that differs between
SteamVR and other runtimes: the full-view transition quads (menu fades / backdrop / bats) got VDXR's head-centre correction
(each eye's quad moved half the eye distance) on every runtime except SteamVR. Pimax places quads from the physical eyes like
SteamVR, so the correction doubled them. SOURCE fix: the shift applies only when the runtime is VirtualDesktop. VD and SteamVR
unchanged. City/AK: VD-only already (hud_eyes_on / g_hudEyes). STATIC: offline check PASS. HEADSET pending (Pimax player).

