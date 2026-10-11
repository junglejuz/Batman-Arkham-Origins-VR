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
