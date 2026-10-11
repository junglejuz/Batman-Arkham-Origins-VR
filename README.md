<p align="center">
  <img src="https://cdn.cloudflare.steamstatic.com/steam/apps/209000/capsule_616x353.jpg" width="616" alt="Batman: Arkham Origins">
</p>

> **Work in progress - not ready to play yet, and there is no release.** Batman: Arkham Origins in PC VR, made the same way as
> [Batman City VR](https://github.com/junglejuz/Batman-Arkham-City-VR) and
> [Batman: Arkham Knight VR](https://github.com/junglejuz/Batman-Arkham-Knight-VR): the game draws both eyes itself
> (native stereo), with per-eye DLSS and OpenXR for Virtual Desktop and SteamVR. The notes below are the developers'
> working notes. The picture above is the game's own Steam store artwork, shown from Steam (Batman: Arkham Origins © Warner Bros.
> Entertainment, Batman and all characters © DC). This is an unofficial fan mod and will need your own copy of the game.

# Batman Origins VR — development build 0.1.0-O1H

Built x86 mod/XInput proxy and x64 DLSS helper for the Steam single-player build
recorded in RE_NOTES.md. This is an initial VR implementation, **not a validated
finished port**. VDXR runs produced native pairs and actual per-eye NGX output;
this update's bat transitions still need runtime and headset validation.

O1H is built and installed on E:. JJ confirmed O1G bats were visible but still
window-sized. Its actual log and paired captures show generated coverage with
zero successful copies/submissions: only the fallback original UI was delivered.
O1H fixes the runtime-image transfer to accept compatible TYPELESS RGBA textures
and explicitly name typed views during format conversion. The actual transfer
helper passes GPU checks on typed sRGB and TYPELESS RGBA/BGRA images; the old
null-view failure is reproduced. Requested/returned formats and failure stages
are now logged. O1H still needs runtime/headset confirmation.

O1E failed JJ's headset check: gameplay bats
were skipped and the first menu reveal remained square. O1F's bats were also
missing in JJ's headset. O1G removes O1F's blanket menu/HUD draw veto and clear:
original drawing is preserved, and only the captured UI's near-black transition
coverage is filtered after a successful XR copy. Coloured menu/HUD content is
submitted underneath the full-view bats, rather than being discarded. Failed
XR setup/copy keeps the untouched original UI and its bats.

The build replays JJ's saved
City bat animation on Origins' exact transition sound events. Title exit starts
with full-view opaque black, then an inverted wipe reveals the menu. Later
events play the bats toward black, hold across loading and reveal the arriving
scene. The first presented frame starts the animation clock, so a render delay
cannot consume the animation unseen. The old black UI backdrop is removed only
while the replacement is available. Titles remain visible before title exit.

Coverage uses near, head-relative per-eye quads sized from the runtime's full
FOV and rendered at headset output resolution independently of DLSS. The saved
distance/enlargement are preserved. BAT TRANSITIONS has size/distance controls,
a 15-second original-view comparison and an opt-in recorder. All three CTests
pass, including actual recorded bat shapes, opaque black, cover/reveal and
loading handoff, preserved menu content, the final GPU composite and an actual
UNORM-to-sRGB texture copy. These are offline checks; all four game contexts
need O1H runtime/headset confirmation. The first two events each automatically
save one paired raw UI / preserved UI / mask sample at >=400 ms. Logs distinguish
generated masks, copied images and frames accepted by xrEndFrame. This local
prototype includes a private captured
game animation at `assets/bat-wipe.bin`; do not include it in a public package.

O1C: native stereo starts automatically; A/B/X/Y, Start or Back skips opening
titles. Origins' fullscreen-triangle scene resolve now starts HUD capture, with
per-eye HUD depth and adjustable width/distance/height. All saved engine/GSA DoF
options are disabled. With DLSS enabled, the launcher temporarily disables stock
AA; successful NGX output is counted separately for each eye.

Implemented and compiled:

- OpenXR headset output, runtime pacing and a controller-operated City-derived
  settings panel; VDXR and SteamVR's native 32-bit OpenXR launch paths.
- Origins-specific native viewport replay and 28-word camera constructor hooks.
  Both eyes use one located head generation and each runtime eye's full pose/lens.
- Independent right-eye view-state allocation through Origins' allocator.
- AFW depth reconstruction and the separate x64 DLSS helper, with per-eye
  histories and camera-derived motion vectors.
- Runtime-recommended eye-size query, 0.6–1.8 scale, window-size handling,
  asymmetric projections, HUD capture, eye captures and live controls.
- Executable/signature guards, ordinary flat launches and reversible installation
  with temporary VR graphics profiles and byte-exact baseline restoration.

Still required: gameplay route coverage beyond the observed checkpoint, native re-entry and
eye-label endurance, Origins post/depth/HUD coverage beyond the observed boundary,
classification, reflection/particle/vendor-shadow fixes, first-person/zoom/marker
parity, full settings/gesture parity, deferred-context coverage, and headset
acceptance of native, AFW, DLSS and both runtimes. A compiled feature is not an
Origins runtime result. No City/Knight engine offsets or shader identities are
enabled in this adapter.

## Run

Close other Steam games and connect the headset. In
`SinglePlayer/Binaries/Win32`, launch **Batman Origins VR.bat**, or
**Batman Origins VR (SteamVR).bat** for SteamVR. Use the normal launcher for VDXR.

The launcher measures the selected runtime's eye recommendation, backs up both
graphics stores, sets temporary render size/windowing/frame pacing and restores
the originals when the game exits. JJ's DoF-off baseline is kept permanently,
with its previous values backed up. Blur and enhanced geometry/AO/shadow options
are preserved. Originals also recover on the next VR launch/uninstall if the
launcher was interrupted. Stock AA changes are temporary when using DLSS.

Native stereo starts automatically on the Origins viewport route observed in
the first live checkpoint. Title/loading routes use a room screen. F10 / hold
both stick clicks opens the panel; AFW is an explicit alternative there.
Face buttons / Start / Back skip the startup movie and keep that press consumed
until release so it cannot also confirm the next menu. Story movies retain
their normal game controls. Startup skip execution is still awaiting a live run.

F12 recentres, F11 toggles tracking, F2 captures both output eyes. A headset
run is required before describing this as playable or correct in stereo.

Log/settings/captures: `%LOCALAPPDATA%/Batman_Origins/`. Commands are written to
`command.txt`, through `tools/Send-Command.ps1` in the source workspace:

```
bo status
bo caller last
bo stereo on
bo screen
bo cam on
bo asym on
bo hud off
bo afw on
bo dlss mode 3
bo bats status
bo bats original
bo bats record
bo reflect on|off|status
bo capture
```

DLSS mode 0=off, 1=DLAA, 2=ultra quality, 3=quality, 4=balanced, 5=performance.
The panel saves the next launch's mode; render-size changes require restart.
Off is the initial default. No performance preset has been accepted in Origins.

## Build / install / undo

```
cmake -S batman_origins -B batman_origins/build -G "Visual Studio 17 2022" -A Win32
cmake --build batman_origins/build --config Release
cmake -S batman_origins/helper64 -B batman_origins/build64 -G "Visual Studio 17 2022" -A x64
cmake --build batman_origins/build64 --config Release
ctest --test-dir batman_origins/build -C Release --output-on-failure
powershell -File batman_origins/tools/Install-VR.ps1
```

MinHook, Dear ImGui and OpenXR are private source copies. The helper uses the
workspace's `akvr_native/third_party/ngx` SDK (DLL CRT), matching the existing
City helper. NVIDIA DLSS runtime is copied locally from that SDK; no redistribution
package has been published. Credit BioShock VR and Luma Framework under the
included notices; the corresponding City/Knight source is the implementation donor.

Uninstall: run `OriginsVR/Uninstall-VR.ps1` next to the installed launcher.
It refuses changed/foreign files and only removes manifest-owned files. It
retains captures, logs and VR settings. No executable binary or shader file is
patched on disk. Install/build identities: `re/evidence/installed-build.json`.
