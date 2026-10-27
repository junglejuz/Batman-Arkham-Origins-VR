## RELEASE 0.1.5 (2026-10-27 night, JJ: "Remove the unnecessary things from the VR panel, including the lens flare stuff deploy to GitHub
## and update the release notes")

Panel tidy (PANELTIDY): LENS FLARES drawer removed (FLATCARDS / FACEGAME always on; `bo flareroll on|off` kept), TESTS drawer loses the
Aim finder and Streak test buttons; AIMWATCH (5 s debug-register watch at the first aim) off by default (settings aim_watch=1 to use).
Version 0.1.5 (CMake, package). Release build in build/ (Win32), 4/4 offline checks pass. Package: make_package.py now drops the old
[glow] section and the superseded level_world key from the shipped settings. Contents since 0.1.4: head aim in first and third person
(AIMSTATE / AIMDEPTH / AIM3P / HUDAIM2), lens-flare cards facing the game camera (FLATCARDS / FACEGAME), LEANBACK, ENTRYHOLD +
ENTRYFADE, FADEMID, LOOKFWD, CAMPOS, FASTTRAVEL, STEREOPOSE, CAMHOLD, lens flares back on (LENSFLAREOFF undone). HEADSET (JJ): head
aim, FACEGAME, ENTRYHOLD; LEANBACK / AIM3P / ENTRYFADE built, not yet confirmed in the headset at release time.

## LEANBACK (2026-10-27 night)

JJ: "When in first person mode and I move my physical head back my virtual camera is moving through Batman's head". The headset's own
movement (scene.cpp delta(eyes)) is added on top of the eye point 25 cm in front of his face; leaning back 25 cm reaches his head.
SOURCE (built, 4/4 checks, installed; previous in backups/pre-LEANBACK-2026-10-27): first_person::clamp_eye - after each eye's final view
is built, in first person its origin may come no nearer than "closest to his face when you lean back" (default 15 cm, never more than
"camera in front of his face") to his head point along the pair's level look; only that component is limited (sideways / up / down free;
both eyes get the same shift = stereo kept). settings [first_person] lean_min_cm.

## AIM3P - head aim in third person (2026-10-27 night)

JJ: "Finally, it's fixed [FLATCARDS - the streak]. Now can you make the head aiming work in third person the same way it does with first
person?" HEADSET 20:2x: FLATCARDS confirmed by JJ.
SOURCE (built, 4/4 checks, installed, dll sha256 D6FCF811...; previous in backups/pre-AIM3P-2026-10-27): first_person.cpp third-person
branch (camValid, also entered now when "aim with your head (third person)" is on): while the left trigger is held (always required in
third person), AIMSTATE steers the controller to body x head (+ AIMDEPTH through the looked-at point, from the game camera's real
position); since the game camera ORBITS Batman with the controller, each pair is drawn from where it would be for the stick's direction:
the camera's offset from his eye point in its own (possibly lagging) frame, laid along the body basis, rotation = body basis. After the
release the same compensation runs until the camera is back within ~0.5 deg of the stick (<= 1.5 s) = no swing back. CAMPOS applied on
top. Panel (third person, FIRST PERSON unticked): "aim with your head while holding the left trigger (third person)", settings
[first_person] look_aim_3p (default 1); TESTS counter "third-person head aim pairs".
Known limit: the orbit pivot is taken as his eye point; if the game's pivot is the shoulder, the view may drift a few cm as the head turns.

## FLATCARDS - 2026-10-27 night, after the 20:08 run

JJ: "Still rotating, what is going on?" LIVE 20:09 census: the additive card batches in this scene are 4-41 cards of a SQUARE 256x256
BC3 picture (vs 776C74B8, indexed 18-246, some with a 256x256 second picture) - all "not aligned": the thin-picture rule (STREAKCARDS /
FACEGAME) never matched the streak's draws, so FACEGAME never ran on them. (ENTRYHOLD fired 20:09:25.)
SOURCE (built, 4/4 checks, installed, dll sha256 58C740C6...; previous in backups/pre-FLATCARDS-2026-10-27): every TEXTURED (picture
>= 16 px) additive triangle-list batch (multiple of 6, <= 3000 indices) gets the GS, FACEGAME for every aligned piece; in the GS a
triangle is rebuilt only if it lies flat to the eye's view (corner depth differences <= 5 % of its size + 0.5 UU = a head-facing card)
and in front - lit meshes / signs pass untouched. Status line now "flare pieces turned N + batches N (...; facing the game camera N)".
Test: streak near a wall, head left / right / roll -> no turning, no cutting into the wall; TESTS FACEGAME off -> turning back.

## FACEGAME + ENTRYFADE - 2026-10-27 night, after the STREAKCARDS run

JJ: "the black is fine, but it needs to fade into the game rather than just cut into it. Also, the streak is still rotating with head
movements left and right, and I can guarantee it is rotating around its y-axis because when there's a wall there, the flare streak
disappears into the wall when it rotates".
= the streak cards stand IN THE WORLD (depth-tested) facing the head-tracked eye view; turning them in the picture (TEXALIGN /
STREAKCARDS) can never stop the card turning in depth. JJ's rule (2026-10-27): game elements must face the GAME camera, never the
head-tracked one (VR_ESSENTIALS §6 MARKERCARD, §3 head-free camera aim).
SOURCE (built, 4/4 checks; rebuild maths checked numerically - card facing a 30-deg-turned head comes back facing the game camera, same
centre and size; installed, dll sha256 2EF9D9E1...; previous in backups/pre-FACEGAME-2026-10-27):
- FACEGAME (flare_roll.cpp GS mode 1, scene.cpp ctor): per eye, the lens terms P00 / P11 / P20 / P21 and the game camera's right / up in
  this eye's view space; the GS takes each card's corners back into eye view space, finds the card centre (longest-edge midpoint), lays
  each corner's offset along the eye's right / up onto the game camera's right / up, projects again; each corner keeps its own NDC
  depth. Applied to the thin-picture card batches (STREAKCARDS); single quads keep the in-picture turn. TESTS switch "streak cards face
  the game camera, not your head (FACEGAME)", counter "cards turned to the game camera".
- ENTRYFADE (transitions.cpp): after the ENTRYHOLD black, a black layer over each finished eye image fades out over 1 s (own tiny alpha
  pipeline, state saved / restored); the HUD panel stays hidden 0.5 s into the fade.

### TEST PLAN FG1 (one launch, JJ drives)
| # | JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | load into gameplay | ENTRYFADE | black, then a 1 s fade into the game |
| 2 | streak light near a wall: head left / right / roll | FACEGAME | streak no longer turns or cuts into the wall; TESTS counter > 0 |
| 3 | TESTS: untick FACEGAME and repeat | A/B | the turning comes back |

## STREAKCARDS + ENTRYHOLD - 2026-10-27 night, after the 19:55 run

JJ: "Okay, I'm done. Did you get it? Also, when entering gameplay, hold the loading screen for another 2 or 3 seconds to avoid showing
game elements loading in when we first enter the game".
LIVE 19:55 census (64 distinct additive draws onto the scene, first 64 only): almost all are DrawIndexed meshes with a 4x4 + square
1024/512/256 picture (lit signs / surfaces, 12-2640 indices); single quads aligned: 2 kinds (6 indices, 1024x1024 + 512x512 BC1);
THIN pictures only in glow draw 63 (indexed 84 = 14 cards, t0 512x128 BC3, vs 80383CF8) and glow draw 1 (indexed 24 = 4 cards,
t0 64x256 BC1, vs 7EED2138) = the streak candidates; never touched before (multi-card batches).
FADEMID 19:55:41: "3D scene back after 3015 ms", "fade-in box seen (whole panel) 16 ms", "gone (panel clear) after 1328 ms".
SOURCE (built, 4/4 checks, installed, dll sha256 8994D34D...; previous in backups/pre-STREAKCARDS-2026-10-27):
- STREAKCARDS (flare_roll.cpp piece()): additive triangle-list batches (multiple of 6, <= 1200 indices) whose first picture is >= 3:1
  are aligned card by card (same GS: each card's centre from its diagonal, its texture axis onto the level). Square-picture meshes
  never. Census line ends "ALIGNED" for them.
- ENTRYHOLD: menu_state keeps the time IsPlayingFullScreenMovie last answered TRUE (outside its 800-line log limit); on the world's
  return after >= 1.5 s, if that movie flag was TRUE during the gap (= a loading screen, not the map), transitions paints every finished
  eye image black and gfx_hud withholds the HUD panel for the set time (panel MENUS AND SCREENS "black after a loading screen (s)",
  default 2.5, settings [screens] entry_hold_s). Hard cut at the end (no fade helper). Log "ENTRYHOLD: after a loading screen".

### TEST PLAN SC1 (one launch, JJ drives)
| # | JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | load into gameplay | ENTRYHOLD | 2.5 s black after the loading screen, no pop-in; log line |
| 2 | streak light: head left / right / roll | STREAKCARDS | streak level; census line 63 / 1 "ALIGNED" |
| 3 | TESTS "hide the flare pieces the mod turns" (only if 2 fails) | which draw | streak gone = ours |

## PIECECENSUS - 2026-10-27 night, after the 19:51 run

JJ: "Nope, it's still happening. I actually think it is in fact rotating around its y-axis in order to face the same direction as the
camera lens". LIVE 19:51: 14006 pieces aligned (one VS, 64x64 sprites), level_world2=1 (JJ ticked the world option), hide test not
used. A card facing the game camera's direction would still land tilted in the eye picture and our texture alignment would level it =
the streak is evidently NOT among the aligned 64x64 pieces (lamp glows).
SOURCE (built, 4/4 checks, installed; previous in backups/pre-PIECECENSUS-2026-10-27): piece() replaces sprite_draw - every distinct
ADDITIVE draw onto the eye-size HDR scene (Draw <= 1000 vertices, DrawIndexed <= 3000 indices, set_draw_indexed_hook / _after) is
logged once after ~40 s of play ("[flareroll] glow draw N: draw|indexed count, topology, t0 / t1 size + format, blend, vs, ALIGNED |
quad, no shader | not a single quad"); single quads drawn indexed (6, list) or as a 6-vertex list are aligned like the strips.
Next: the census names the streak's draw (expect a wide, thin texture) -> align / re-orient exactly that one.

## ALWAYSALIGN - 2026-10-27 night, after the 19:46 run

JJ: "It only improves it by a couple of degrees it's still rotating" (TEXALIGN2 + LEVELWORLD build).
LIVE 19:47: flare piece VS outputs = float2 TEXCOORD0 (the texture coordinate) + five float4s - so TEXALIGN's first candidate WAS right.
Cause (SOURCE): flare_roll only touched anything while the eye's tilt was over 0.2 deg (g_active). With "level with the real room"
(19:40) and the head level that is never -> TEXALIGN never ran and the game's own world-following turn stayed; with "level with the
game world" (19:47) it ran but aligned the pieces to that same world tilt -> "a couple of degrees".
SOURCE (built, 4/4 checks, installed, dll sha256 4BB7023B...; previous in backups/pre-ALWAYSALIGN-2026-10-27): flare pieces aligned in
EVERY eye picture (g_spritePass), default level = the real room (settings key level_world2, default 0); TESTS "hide the flare pieces
the mod turns" (drops them; counter) to prove the streak is one of them.
Test: streak light, head left / right / roll -> level in the view; if not, TESTS hide switch: streak gone = ours, else drawn elsewhere.

## TEXALIGN2 + LEVELWORLD - 2026-10-27 night, after the 19:40 run

JJ: "When I turn my head left, the streak rotates clockwise. When I turn my head right, the streak rotates anticlockwise" (19:40 run,
TEXALIGN build). LIVE 19:40: FADEMID works - "fade-in box seen (whole panel) 437 ms / 16 ms after the 3D scene came back - panel
held", "gone (3 s limit / panel clear)"; flare pieces turned 3410, GS built.
Computed (numpy): game camera pitch +9 deg (FLIPWATCH 19:09 "game camera pitch 9.1"), head 40 deg left -> the game WORLD's horizon
tilts +5.8 deg clockwise in the view; right -> -5.8 = exactly JJ's directions: the streak stays level with the game world = TEXALIGN
changed nothing (its first TEXCOORD is evidently not the texture coordinate - constant per piece -> fallback to the head roll alone).
SOURCE (built, 4/4 checks, sample GS compiled with fxc; install queued until the game exits - the game was running, DLL locked;
previous in backups/pre-TEXALIGN2-2026-10-27):
- TEXALIGN2: every float pair of every TEXCOORD output is a candidate; the first that spreads like a texture coordinate over the
  triangle (0.02 < |det| < 50) gives the texture x axis. Log once per flare VS: "[flareroll] flare piece shader ... outputs: struct V".
- LEVELWORLD: the target level is the GAME world's horizon (against the game camera's own, FLARESPRITE's first angle) by default;
  panel LENS FLARES "level with the game world (off = level with the real room)" switches to HEADROLL. settings [lens_flares]
  level_world.

### TEST PLAN TA2 (one launch, JJ drives)
| # | JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | streak light: head left / right, roll | TEXALIGN2 + LEVELWORLD | streak stays put against the world |
| 2 | panel LENS FLARES: untick "level with the game world", repeat | the other reference | JJ picks the one that looks right |
Follow-up: still turning -> the "outputs" log line names the real texture coordinate (or there is none: the turn is in the PS).

## TEXALIGN + FADEMID - 2026-10-27 night, after the 19:23 run

JJ: "Looking left and right still makes the streak rotate along its Z axis. The grapple batclaw is a lot more accurate now. There is
still a square in the middle of the field of view when entering gameplay. It could be attached to the HUD element information that
appears".
LIVE 19:24: aimdepth reads 2028 (sky 6), last distance 1753 UU = AIMDEPTH working (JJ: "a lot more accurate"). HEADROLL in place: with
the head level the flare turn is ZERO, so the remaining turn on sideways looks is the GAME's own (its pieces are turned as the light
crosses the picture). FADEHOLD never armed on the gameplay entry: world pass off 19:24:03.878 -> on 19:24:06.845 = 2967 ms, just under
the 3 s trigger (the 19:09:47 trigger was the boot / main menu, not gameplay). LOADBURST 19:09 HUD sheet is RGB (no alpha) - a dark
box on the panel cannot be seen in it.
SOURCE (built, 4/4 checks, generated GS sample compiled with fxc gs_4_0, alignment maths checked numerically; installed, dll sha256
DF6B95AE...; previous in backups/pre-TEXALIGN-2026-10-27):
- TEXALIGN (flare_roll.cpp GS): each flare piece's texture x axis on the picture (d position / d u from the first float2+ TEXCOORD of
  its three corners) is turned onto the head-level direction (mod 180 deg), about the quad's centre - the piece drawn as authored,
  level, wherever it is and however the game turned it; no usable texture coordinate = the head-roll turn alone.
- FADEMID: fade watch armed after >= 1.5 s without a 3D scene (was 3 s); four more probe points around the panel centre - a box over
  the whole panel (City) OR over its middle (centre + 3 of 4 inner points dark and covered) holds the panel; log "fade-in box seen
  (whole panel / middle) N ms after ..." then "gone (...)".

### TEST PLAN TA1 (one launch, JJ drives)
| # | JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | load into gameplay from the main menu | FADEMID | no square; log "3D scene back", "fade-in box seen", "gone" |
| 2 | look at a streak light, turn the head left / right and roll | TEXALIGN | streak stays level |
| 3 | look at other glows (lamps, signs) | nothing broken | no oddly turned sprites |
Follow-ups: 1 "no box within 0.7 s" but JJ still sees it -> the square is not dark / not on the panel centre: panel picture WITH alpha
(gfx_hud::request_panel_sequence) on the next load; 2 still turning -> the piece's TEXCOORD0 is not its texture coordinate (log a GS
census of the VS outputs).

## HEADROLL + AIMDEPTH + HUDAIM2 - 2026-10-27 night, after the 19:09 run

JJ: "The anamorphic glow is no longer rotating when I roll my head, which is good, but it is rotating when I look left and right with my
head. So we're halfway there. The batclaw is still offset from the reticle to up and right of the actual reticle".
LIVE 19:10: FLARESPRITE works - "flare piece" = 64x64 BC1-sRGB sprites, blend ONE/ONE onto the 1328x1424 fp16 scene, geometry shader
built (1), 2206 pieces turned in 30 s. AIMPOV wrote both view records ("aim now starts at your eyes") and changed nothing = the game's
trace does not start from those records. Lens L(l -49.5 r 35.5 u 39.4 d -50.5) R(mirrored): an off-centre-lens cause would point
right AND DOWN by 11-13 deg - ruled out; "a little up and right" = the over-the-shoulder camera's parallax (+ the HUD's -1.4 deg
height offset for the "up" part).
Looked up: Batman City markers.cpp AIMDEPTH / BATMAN_CITY_PLAN AIMDEPTH (depth under the centre ray), VR_ESSENTIALS §6 "Centre aim
reticles: find their point by ray-marching the depth buffer".
SOURCE (built, 4/4 checks, installed 2026-10-27 night, dll sha256 767EF057...; previous in backups/pre-AIMDEPTH-2026-10-27):
- HEADROLL (scene.cpp): the flare tilt is the headset eye's roll against real-world level (OpenXR up), not against the game world's
  horizon - with the game camera pitched, a sideways look rolled the view against that horizon.
- AIMDEPTH (new aim_depth.cpp): at the left eye's tone-map step a 1-pixel shader reads the scene depth (gfx_hud::scene_depth) under
  the eye's centre ray into a 1x1 R32F target -> staging, read one picture later (DO_NOT_WAIT); z = m32 / (d - m22) (the eye's own
  lens terms); world point = eye origin + forward x z (sky -> far; nearer than 1 m ignored = his arm). AIMSTATE then turns the
  controller so the GAME camera's ray (this pair's camera position) passes through that point (only if within ~25 deg of the look).
  AIMPOV removed. Status "aimdepth: reads N (sky, too near) ... last distance"; TESTS "aimed through the looked-at point N".
- HUDAIM2 (openxr_runtime.cpp): while the HUD rides the head for aiming, it sits exactly on the head's aim (no -0.10 m height).

### TEST PLAN AD1 (one launch, JJ drives)
| # | JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | look at a streak light, turn the head left / right, then roll | HEADROLL | streak level in both |
| 2 | hold LT, put the reticle on a near grapple point (5 m) and a far one (20 m+) with the head, fire | AIMDEPTH | lands on the reticle at both distances; status "aimdepth: reads" > 0, distances plausible |
| 3 | aim at the sky / far away | no point | still fires along the look |
Follow-ups: "reads 0" -> no tone-map match or no depth (log); lands consistently off by a constant -> the trace starts elsewhere than
the game camera position (AIMWATCH readers at fire time).

## AIMPOV + FLARESPRITE - 2026-10-27 night, after the 18:04 run

JJ: "The anamorphic glow is still not fixed, it's still rotating along its z-axis when I roll my head. The batclaw launch is now much
better. It is launching where the head is looking, but it seems to be a little bit above and to the right of the actual reticle."
LIVE 18:05: AIMSTATE works (head aim active / released lines, body pitch -36). flareroll census (one tilted eye picture): only TWO
full-screen steps - draw 3 -> 332x356 rgba16f from the scene (real depth write, quarter D24S8) and the tone map reading the scene
alone; "turned back 0" all run = this game's current frame has no quarter-size streak chain at all (the 2026-10-11 dump's chain is
gone or elsewhere) -> the chain turn is pointless and is switched off.
STATIC (framedump_044343): five single-quad Draw(4) strips onto the HDR scene T15 after translucency, each with its own flare
texture (512x512 / 256x256 BC1 / BC3 + 64x64 + scene copy), caller 0x630DB2 (the translucency drawer) = the lens-flare pieces.
SOURCE (built, 4/4 checks, installed 2026-10-27 night, dll sha256 FCA6EC95...; previous dll + manifest in backups/pre-FLARESPRITE-2026-10-27):
- AIMPOV (first_person.cpp): the game aims from ITS camera (over the right shoulder, above / behind his head) along the look we put
  in the controller - a parallel ray from another place lands above / right of the reticle. After the pair's last eye, the camera's
  view record(s) (controller +0x370 -> +0x3D8 / +0x41C, found by POVAIM in three launches) get our eye point + the head's look, only
  while they still equal this pair's game camera (2 UU / 1.5 deg). Log "the game's aim now starts at your eyes"; TESTS line
  "aim start at your eyes: written / not matching".
- FLARESPRITE (flare_roll.cpp): CreateVertexShader hooked at start-up (throwaway device, vtable 12; every VS output signature noted,
  AKVR hudsplit.cpp MKGS method). A non-indexed 4-vertex strip with ADDITIVE blend (dest ONE) onto an eye-size HDR target gets a
  generated geometry shader that turns each quad about its own centre (the longest edge of each triangle = the quad's diagonal) by
  the eye's tilt; full-screen quads untouched; unbound right after the draw. Log "[flareroll] flare piece N: ..." (first 16, with
  inputs and blend) and "CreateVertexShader hooked". Census now waits ~40 s into play. Chain turn kept behind a TESTS checkbox.

### TEST PLAN FS1 (one launch, JJ drives, nothing to press)
| # | JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | play ~1 min, then look at a streak light and tilt the head | FLARESPRITE | streak stays level; log "flare piece" lines + "flare pieces turned N" > 0 |
| 2 | hold the left trigger, aim the batclaw at a small grapple point with the head, fire | AIMPOV | lands on the reticle; log "aim now starts at your eyes" |
| 3 | look around normally (no trigger) | nothing else turned | no other glow / sprite looks wrong |
Follow-ups: 1 no "flare piece" lines -> the pieces are indexed draws or another shape -> the census lines name them; pieces turned
but the streak still tilts -> the streak is not a single quad (census) ; 2 "not matching" counts -> the record moved (log).

## AIMSTATE / FLAREROLL DEPTHOFF + CENSUS - 2026-10-27 night, after the 17:53 / 17:54 runs

JJ: "The anamorphic glow is still rotating with my head rolls and the bat claw is still not firing towards where I am looking. It is
firing in the direction of Batman's body instead of using the head tracking".
LIVE 17:55: "the controller FEEDS the camera (its view jumped onto the head's look)" = Controller.Rotation is the camera's own STATE
(classic UE3: the stick turns the controller, the camera and the aiming body follow it); writing the view record only moved the
reticle. AIMWATCH: 54 readers (controller yaw: exe+0x2F0E3, +0x3237CE, +0x328BDF, +0xAF3278, +0xAF3A47, +0xA1FBD1, +0xA288BB, ...;
Pawn.Rotation: exe+0xEE994 x6853, +0x95C73E, +0xA21DCD, ...) - kept for the static follow-up if needed.
LIVE 17:54 / 17:55 flareroll: "steps 0 ... depth-tested steps left 2985 / 6545" = every candidate step had a depth / stencil test and
was skipped (framedump_044343: the four streak passes write T524 with the quarter D24S8 T457 bound, "T524: 4 / 4" depth-tested).
SOURCE (built, 4/4 checks, installed 2026-10-27 night, dll sha256 77817B38...; previous dll + manifest in backups/pre-AIMSTATE-2026-10-27):
- AIMSTATE (first_person.cpp, replaces POVAIM + AIMCTRL; the view-record finder is gone): while the left trigger is held in first
  person, each pair: body = Controller.Rotation - our last yaw contribution (stick turns kept), body pitch = the pitch when the
  trigger went down (no drift, Asylum AIMLEVEL), controller = body x head; both eyes render from the body basis (no double head);
  release writes the body back. Log "head aim active: Batman's controller follows your look" / "head aim released (...)".
  AIMWATCH now watches Controller / Pawn rotation yaw + pitch.
- FLAREROLL DEPTHOFF: a quarter-size step with a REAL depth / stencil test (not ALWAYS / KEEP) is turned like the others; its depth
  buffer is detached for that one draw and put back right after (draw-after hook). Counter renamed "depth-tested steps (depth
  detached)".
- FLAREROLL CENSUS: the first eye picture of a launch tilted more than 3 deg logs every full-screen step ("[flareroll]  draw ...":
  target size / format, CHAIN / quarter, depth + stencil state, real test, the four inputs with "L" for level) - if the streak still
  tilts, this names the real chain without another test run.

### TEST PLAN AIM3 (one launch, JJ drives, nothing to press)
| # | JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | first person, look at a light, tilt the head well to one side for a few seconds | FLAREROLL + census | streaks stay level; log "census ... census end", steps / turned counts > 0 |
| 2 | hold the left trigger, look (head only) at a grapple point, fire the batclaw; a batarang at a thug | AIMSTATE | gadget goes where he looks; Batman turns with the look |
| 3 | while holding, turn with the stick; release | no jumps | view turns normally; on release the view stays where it was |
| 4 | pause menu / map while holding | release on menus | log "head aim released (menu ...)" |
Follow-ups: 2 still straight -> AIMWATCH pawn readers at fire time (static); 1 still tilting -> the census lines show which steps
draw the streak (format / size / depth) -> one targeted rebuild.

## AIMCTRL + AIMWATCH / FLAREROLL (glow removed, lens flares back) - 2026-10-27 evening, after the 17:35 run

JJ: "Head aiming is now working, but when launching the batclaw or other weapons, it's not launching towards the reticle where the head
is aiming. It's launching straight ahead. Also, the glow is not working still. I say we remove it and put back the original lens flare
and fix it so that lens flares do not rotate with head rolls".
Looked up: memory hw-read-watch-finds-readers (SWBF2 probe.cpp DR watch), ak-lens-flare-is-two-shaders; this file STREAKROLL (the
planned streak fix: rotate the streak's sampling by the eye's roll); VR_ESSENTIALS §3 (camera-aim shaders / head-free camera).
LIVE 17:36:23: "view record found at controller +0x370 -> +0x3D8" and "-> +0x41C" (value after it 55.4 = FOV), head aim active,
no "FEEDS" / "did not rebuild" lines = the POV writes are safe, but gadgets still fly along the stick camera.
STATIC (framedump_044343, 2026-10-11, lens flares on): after the scene: half (764x820) -> quarter 384x412 R16G16B16A16_UNORM (copied)
-> three IDENTICAL full-screen passes into 382x410 R16G16B16A16_FLOAT, each copied back and re-read (an iterative streak; same
constants every pass), a fourth adding another quarter picture, two 2D bloom passes, then the tone-map step reads scene + both quarter
results + the 256x16 colour table. Shader identities are not in that dump.
SOURCE (built, 4/4 checks, installed 2026-10-27 evening: dll sha256 E02E04B9..., Graphics-Profile.ps1 B1ABE127...; previous dll,
script, manifest and JJ's GFXSettings xml in backups/pre-FLAREROLL-2026-10-27):
- AIMCTRL (first_person.cpp): with the view records, Controller.Rotation (+0x54) gets the head's look too (UE3 fires along
  GetBaseAimRotation = Controller.Rotation), only while it equals this pair's camera heading; per record the value before our write
  is put back if not rebuilt; if the view then jumps onto the written look, only the controller is refused ("the controller FEEDS
  the camera").
- AIMWATCH: first head-aim write of a launch -> 5 s hardware watch (Dr0-3, read / write, all threads, a helper thread arms and
  disarms) on view record 1 / 2 yaw, Controller.Rotation yaw, Pawn.Rotation yaw; log "[bo] AIMWATCH <field>: after <eip> (exe+0x..)
  xN" = the code that reads them while JJ aims and fires. If the gadget still flies straight, the reader near the batclaw fire is
  the next target (static, no new test). settings.ini [first_person] aim_watch=0 switches it off.
- GLOW removed (glow.cpp / .h retired to the backup folder; panel drawer gone; [glow] ini section now unused).
- Lens flares back: Graphics-Profile.ps1 no longer forces LensFlares off (store or engine ini); JJ's GFXSettings store set back to
  LensFlares true (FIX_CHANGES "FLAREROLL").
- FLAREROLL (flare_roll.cpp): per eye, the tilt of the eye against the game camera's horizon (scene.cpp ctor, picture angle). In
  every quarter-size full-screen step (target R16G16B16A16, at most 1/3 of the eye width, no depth / stencil test) each RGBA16 input
  still in the eye picture is replaced by a copy turned LEVEL; its target (and game copies of it, copy tap) count as level; any
  other draw reading a level picture (the tone-map step) gets a copy turned back to the eye's tilt. The game's streak is thus drawn
  horizontal in a level picture. Head level (< 0.2 deg) = nothing touched. Panel LENS FLARES: "keep lens-flare streaks level when
  you tilt your head" (settings.ini [lens_flares] level, default 1); command "bo flareroll on|off"; status line "flareroll ON:
  steps N, turned in / back". Known costs: two extra bilinear resamples of the quarter-size bloom / streak (slightly softer),
  black corners for very large tilts.

### TEST PLAN AIM2 (one launch, JJ drives, nothing to press)
| # | JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | first person, look at a light, tilt the head left / right | FLAREROLL | streaks stay level with the world; with the panel switch off they tilt again; log steps / turned counts > 0 |
| 2 | hold the left trigger, look at a grapple point with the head only, fire the batclaw; then a batarang at a thug | AIMCTRL | the gadget goes where he looks; log "+ Controller.Rotation", no "FEEDS" |
| 3 | (automatic during 2) | AIMWATCH | "[bo] AIMWATCH done ... N distinct instructions" + one line each |
| 4 | turn the camera with the stick while holding the trigger | no drift | the view turns normally; no jumps |
Follow-ups: 2 fails -> the AIMWATCH readers that run only around the fire name the aim source (static, Ghidra on the exe) -> write
there. 1 fails (streak still tilts) -> "steps 0" = detection missed (log status) -> a frame dump names the chain; streaks doubled or
misplaced -> switch off in the panel, report.

## GLOWEYE / POVAIM / HUDAIM / LOOKFWD (2026-10-27 evening, JJ after the 16:50-16:57 runs)

JJ: "our added glows are only showing in the left eye, and turn on and off as you move your head around. Head aiming in first person
is still not working. The HUD should move with my head when holding down the left Trigger. Also I should not be able to spin the
camera around to look directly at batman's face up close while in first person mode". (The TESTS "Aim finder" was not pressed.)
Looked up: VR_ESSENTIALS §5 (ACOVR P5T: an engine may present a view's picture during the NEXT view's job - tag by the drawn view,
not by Present) and §2 (anything cached per frame must run per eye); §6 HUD head-locked mode (runtime "attach UI to head");
AKVR camera.cpp AIMHEAD/AIMTRIGGER (head written into the camera's own final block, left trigger only); memory
arkham-first-person-asylum-origins, batman-city-first-person (head never hidden in any Arkham mod - the lens sits in front of it).
Evidence (LIVE, today's log + EYEBURST 16:54):
- Glow: status "passes 4094 ... fallback 4094" = one glow per PRESENT; the burst's present times alternate ~13 ms / ~2 ms (sign -1
  long, sign +1 short, 424 of 449) = BOTH eyes are drawn, then both presented. "Once per present" glowed only the first-drawn eye;
  its red lights are visibly larger than the other eye's (pairs 40/300/600/880); when the timing shifts (25 long gaps for +1) the
  glow jumps eye = "on and off". Every pass also used the fallback input (the scene leader never matched) but the input never
  switched, so that is not the cause.
- Head aim: "Controller.Rotation at +0x54 confirmed", "head aim active" at 16:54:37 and 16:57:04, no effect on gadgets = the
  controller is a copy of the camera.
SOURCE (built 4/4, installed 2026-10-27 evening, sha256 20514E41..., old DLL + manifest in backups/pre-POVAIM-2026-10-27):
- GLOWEYE (glow.cpp): once per eye RENDER - scene.cpp begin_detour (the engine's view-family begin) counts eye passes; the present
  count only if that hook never ran.
- POVAIM (first_person.cpp, replaces the controller write + AIMLEVEL body basis): UE3 aims from Camera.CameraCache.POV (Location,
  Rotation, FOV via GetPlayerViewPoint), rebuilt by the camera every tick. The mod finds it itself: among the controller and up to
  160 objects it points to, a float3 = the game camera's position (2 UU) followed by a rotator = its yaw / pitch (1.5 deg), on 20
  checks in a row while the camera moves (a one-frame-old copy fails); log "the camera's view record found at controller +0x.. ->
  +0x..". While aiming (AIMTRIGGER: left trigger) the head's look (game camera x headset) is written into that rotation AFTER the
  pair's LAST eye has read it, so the next tick's gadget aim uses it and the camera overwrites it - nothing accumulates, the
  rendered view is untouched. Guards: written only where the record equals this pair's camera; next pair, if the record still
  holds our write (not rebuilt) the camera's value goes back before the second eye reads it and the first eye is drawn from the
  camera's own rotation ("did not rebuild"); if the game camera jumped onto our written look the record feeds the camera -> put
  back, never written again ("FEEDS the camera"). ~30 s of moving camera without a record -> "no view record found" + up to 6
  "position seen with other rotations" lines for the next step.
- HUDAIM (openxr_runtime.cpp + scene.cpp): while the left trigger is held in gameplay (first and third person) the HUD rides the
  head; on release it returns to its room place. Panel HUD: "HUD follows your head while you hold the left trigger (aiming)",
  settings.ini [hud] on_head_aiming (default 1).
- LOOKFWD (first_person.cpp, replaces FACEFWD): the lens sits "camera in front of his face" out from his head along where you LOOK
  (game camera x headset, level, per pair), so his head is always behind the lens however the stick or your head turns. FACEFWD
  put it in front of his FACE, so turning the camera round showed his face up close. Note: turning the head now swings the lens
  round his head at that radius (25 cm); if that feels like orbiting, lower "camera in front of his face".

### TEST PLAN POV1 (one launch, JJ drives, nothing to press for me)

| # | JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | Load a save, first person on, look at red / bright lights and move the head | GLOWEYE | glow the same in both eyes, steady; log status "passes" ~ 2 per stereo pair |
| 2 | Walk and turn the camera with the stick for ~10 s | POVAIM finds the record | log "view record found" (or "no view record found" + candidates) |
| 3 | Hold the left trigger, look (head only) at a grapple point / thug, fire the batclaw / batarang | head aim + HUDAIM | gadget goes where he looks; HUD follows the head while held, returns on release; log "head aim active", no "FEEDS the camera" |
| 4 | Turn the camera all the way round with the stick, then turn the head behind | LOOKFWD | never his face; no teeth |
| 5 | Pause menu, map, quit | gates | HUD back in its place in menus; clean exit |
Follow-ups: 2 fails -> the logged "other rotations" lines name the right record (sign / layout) -> one rebuild. 3 aims but the view
jumps for a frame -> "did not rebuild" count in TESTS -> write later in the frame. 4 orbit feels wrong -> push only by the stick
(game camera) and keep the head's turn in place.

## HEADAIM2 / AIMFIND / FACEFWD / GLOWSCENE (2026-10-27, after JJ's first try)

JJ: "the glow flickers on and off ... when you move your head around just in small increments ... just which angle your head is looking
at it from"; "The first person camera is inside Batman's head. I can see his teeth. The camera needs to be in front of the head and
attach to it"; "The head aiming is still not working. When you pull the left trigger to activate the [bat]claw or other weapons, it
should activate the head aiming".
LIVE log 00:49:05: "Controller.Rotation at +0x54 confirmed" + "head aim active" and writes every pair, yet no effect on gadget aim =
the game's camera keeps its own rotation and copies it into the controller (written value overwritten). Looked up: AKVR camera.cpp
AIMHEAD / AIMTRIGGER (writes the head into the CAMERA's finalize block - the rotation the game reads back for aiming - and only while the
left trigger is held, akvr_gamepad_left_trigger_ms < 300).
- AIMFIND (TESTS "Aim finder (20 s)"): 10 samples/s for 20 s of the controller + every game object it points to (0xC00 bytes each, up
  to 64) with the game camera's yaw / pitch -> captures\aimfinder_<tick>.bin; tools/aimfinder.py lists fields that follow yaw / pitch
  (rotator units, float degrees, float radians; >= 90 % of samples within 2 deg). Script tested on a synthetic file: the 3 planted
  fields found, no false ones. Next: write the head into the found camera field(s) like AKVR.
- AIMTRIGGER: "only while holding the left trigger (gadgets, batclaw)" (default on; [first_person] look_aim_trigger), from
  input::last_composed_triggers (physical pad and composed controllers); 0.3 s hold after release.
- FACEFWD: the 25 cm forward push now follows Batman's facing (Pawn.Rotation +0x54, used only while its pitch < 30 deg) instead of the
  camera's forward - standing still, the camera can turn away from him and the push went back into his head (teeth).
- GLOWSCENE: the glow input must be hud_capture's scene vote leader (the picture the world was drawn into), a generic screen-size
  fp16 input only as a counted fallback; status adds "input switched N, without a world leader N, fallback N".
Built 4/4, installed 2026-10-27 (sha256 06433DB9...). City: the same FACEFWD / AIMTRIGGER / AIMFIND will follow once Origins' finder
shows the field (City's head aim build writes the controller like Origins' first try).
Next launch (JJ): press TESTS "Aim finder (20 s)", close the panel, turn the camera all the way round with the right stick (third
person is fine); check the first-person camera position and the glow; tell me.

## HEADAIM for Origins (JJ 2026-10-27: "head aiming is not working in first-person mode ... I'm talking about origins")

Looked up: batman_asylum BUILD_PLAN HEADAIM + AIMLEVEL (Godoy301 PR #1: write the head's look into Controller.Rotation, remove our
own contribution each frame, render both eyes from the body basis; body pitch level), this file FIRSTPERSON offsets, memory
contributor-pr-merge-base / arkham-first-person-asylum-origins.
SOURCE (first_person.cpp HEADAIM): Controller.Rotation ASSUMED at Actor +0x54 (Origins Location +0x48 + 0xC, as Asylum 0x54 -> 0x60;
UE3 Actor: Location then Rotation). AIMCHECK: no write until the value's yaw matches the game camera's heading within 4 deg for 30
frames in a row (log "Controller.Rotation at +0x54 confirmed" / "does NOT match ... head aim stays off" after 600 misses). Per pair
(first eye): body yaw = controller yaw - our last contribution (stick turns kept), body pitch = 0; target = body basis x head rotation
(the same z-flipped quaternion matrix scene.cpp delta() renders with); written + read back; both eyes render from the body basis.
Released (our yaw / pitch taken out, pitch level) on menus, no Batman, camera far, first person or aim off. No stick filter: with
the body pitch forced level each pair, stick up / down has no lasting effect. Panel: FIRST PERSON -> "aim with your head", status
text (checking / active / not available), TESTS counters. settings.ini [first_person] look_aim (default 1).
GLOWRANGE in the same build: strength 0-3 (was 0.40), threshold down to 0.02 (was 0.2) - JJ: glow "not very obvious, even at max
intensity and lowest threshold" (log: 28538 passes, attached fine). Built 4/4, installed 2026-10-27 (sha256 30041048...). HEADSET
pending: first look for the "confirmed" line, then gadgets aim where JJ looks.
FADEHOLD LIVE 00:39:20-21: "3D scene back after 3015 ms" -> "fade-in box gone (panel clear) after 985 ms" = fired on that load.

## GLOW (JJ 2026-10-27: "Keep lens flares removed and add a glow setting to the overlay so we can dial in a nice glow. Maybe it
## should be similar to the one we added to Arkham City")

HEADSET 00:33 (JJ: "the [streak] that was there before is now gone"): the anamorphic streaks belong to the game's LensFlares option
(GSA LensFlares false removed them) -> STREAKROLL dropped (streak test button stays in TESTS, unused); lens flares stay off.
Looked up: batman_city glow.cpp + BATMAN_CITY_PLAN GLOW / NODOF / GAMEGLOW. City's offered controls (game's glow strength = light-shaft
buffer, smooth edges, bright-light limit, light beams) need City's decoded combine pass - not known in Origins. City's OWN bloom
(glow_mode 2: Karis prefilter, 13-tap down, tent up; kept, never offered) is engine-independent -> ported as src/game/origins/glow.cpp.
SOURCE: at the first full-screen Draw(3) per present whose target is 8-bit, screen-size and one PS input is the screen-size fp16
scene (= the tone-map step; it reads the HDR scene), the glow pyramid is built from that scene and added ONE/ONE onto the scene
texture itself (own RTV), then the game's draw runs unchanged -> grading / tone mapping treat the glow like light; each eye's own
scene = glow at the light's depth. Saves / restores RT, blend, depth, raster, viewport, all shader stages, PS b0 / s0 / t0..t7.
Panel GLOW drawer: "glow around bright lights" (default on), strength 0.06 (0-0.40), "which lights glow" threshold 1.0 (0.2-4),
size 6 levels (3-7), "default glow"; settings.ini [glow]; `bo glow on|off|strength|threshold|size`; status line
"[bo] glow ON: ... passes N" (+ "[glow] attached to the tone-map step: scene WxH fmt -> picture WxH fmt" once). Shaders checked
with fxc (all 5 compile); built, 4/4 checks; installs at game exit. HEADSET pending; if "passes 0" the tone-map guess is wrong
(it may be indexed / 4 vertices) -> a frame dump of gameplay names the real step.

## TEST PLAN STREAKROLL + CAMHOLD + FADEHOLD + LENSFLAREOFF (one launch, JJ drives, 2026-10-27)

JJ 2026-10-27: "I was talking about actual anamorphic blooms which were at object depth, not lens or HUD depth, so they are fine to
keep, but I just don't want them rotating when I rotate my head" (lens flares off is fine).
Looked up: playbook docs grep roll/streak/blur: nothing on screen-space directional effects under head roll; VR_ESSENTIALS §3
"Shaders that use the camera's AIM" (AKN SUITGLOSS3: give such draws the head-free camera's rows) = the same family; Knight
FLAREOFF (Luma shader names, swap at creation); memory ak-lens-flare-is-two-shaders.
Cause (expected): the streak is a horizontal blur in SCREEN space (sample offsets along +-x); head roll rolls the screen, so the
streak rolls. Fix shape: per eye, rotate the streak pass's sample offsets by -(eye roll vs the game camera), aspect-corrected -
patch the offsets where the draw reads them (constant buffer at Unmap / per-draw copy, VR_ESSENTIALS §3 "patch the copy the draw
binds"), or replace the shader if they are literals.

JJ 2026-10-27: "never make me say go and do things interactively with you. Create buttons in the panel" -> STREAKTEST is a panel
button (TESTS drawer "Streak test (30 s)"): dumps (`frame_inspector::arm(3,2)`, cb mode, 2 present windows) at 6 s (head level) and
18 s (head tilted ~30 deg from 10 s), each logged with the head's roll ("[bo] STREAKTEST dump N ... head roll X deg"); the panel
shows the running step. Built + installed 2026-10-27 (dll 3D0C32AF, Graphics-Profile.ps1 C3F04CF7 - LensFlares now optional in the
GSA store: the write-denied test fixture has none and the strict version threw = would have stopped VR launch for such a player).

| # | JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | load save | FADEHOLD | no black square; log "fade-in box gone ... after N ms" |
| 2 | panel TESTS -> "Streak test (30 s)", close the panel, look at a light: level, from 10 s tilted | which passes draw the streak; offsets do not follow the roll | dumps: small-RT full-screen chain with +-x offsets, same in both dumps; logged rolls ~0 and ~30 |
| 3 | pause open / close x3, a few seconds between | CAMHOLD | JJ: no flicker; log "CAMHOLD ... (menu context)" lines + counts |
| 4 | look at lights | LENSFLAREOFF | JJ: lens flares gone, anamorphic streaks still there |
Dumps deleted after analysis (memory clear-recordings-when-done).

## TEST PLAN MENUFLASH + PAUSEDIRT (JJ 2026-10-26: "on the main menu ... the occasional black frame flickering same thing happens in
## the pause menu I'm getting black flashes and also there is a dirt layer HUD element that's stuck to your face that should probably be
## removed from the pause menu")

Looked up: this file MAPMONO / MT1-MT2 / EYB1 (Origins' earlier "full field of view blackout for one frame" = untagged mono presents
captured into the left eye with a stale pose, fixed ONLY for map-type screens - pause and main menu are pause-type, not covered);
VIEWDEBOUNCE3; memory batman-city-hud-overlays (City PAUSEDIRT / title grime: grime removed = white in place of the dirt) and
arkham-map-flicker-wrong-signal; VR_DEBUG_TOOLKIT.md (capture first). Log 23:44-23:45 (main menu 25 s, pause 13 s): none of our
black rules fired (no "all-black", no BOOTBLACK after title, no camera-mode toggles, no-pose 0, MAPMONO skips 0).
Ranked causes for the flashes: (1) untagged mono presents in pause-type menus captured into the left eye (MAPMONO's cause, not
covered here); (2) a held / stale eye image on a frame without its pair; (3) Virtual Desktop dropping a frame. EYEBURST tells (1)/(2)
(flash inside our eye pictures, one eye) from (3) (eye pictures clean).
PAUSEDIRT (SOURCE, built 2026-10-26, sha256 7B8D1046..., installed): City's grime pass signature swapped with white (`bo pausedirt
on|off`, default on) + a census of every distinct full-screen Draw(3) picture set while a pause-type menu is up.

| # | JJ does | I trigger | what it proves | how to judge |
|---|---|---|---|---|
| 1 | main menu, look around 13 s | `bo eyeburst 2400` | flashes in our eye pictures? | tools/eyeburst_view.py: flash frames, which eye |
| 2 | pause menu, look around 13 s | `bo eyeburst 2400` | same in pause | same |
| 3 | pause menu: dirt still on the face? | - | PAUSEDIRT match | log "PAUSEDIRT: the pause grime pass found"; else census lines name Origins' pass |
| 4 | pause: `bo pausedirt off` 5 s, then on | (switch) | the swap is what removes it | JJ: dirt returns, then goes |

### MENUFLASH RESULTS run 23:53-00:04 (JJ drove; LIVE)
- PAUSEDIRT census 23:55:26 (pause opening): `t0 4x4 fmt 28 | t1 1328x1424 fmt 10 | t2 256x256 fmt 72` = City's grime pass with a
  256 dirt (City rule wanted >= 512 -> no match, "grime passes seen 0"). Rule now >= 128 square (4x4 + fp16 scene stays the key).
- EYEBURST 23:54 title screen 13 s: no flash (max 0.49). 00:03 + 00:03:58 main menu 2 x 13 s: no flash (max 0.85).
- EYEBURST 23:56 PAUSE 13 s: flashes INSIDE our eye pictures (before DLSS), scores 23 / 15 / 14 / 13 ..., always BOTH eyes of one
  pair (same gen), in clusters (gens 11771-11777, 11801, 11839, 11864, 11921). Flash pictures (flash_1095 / 1034 / 1220): a view from
  ABOVE Batman looking straight DOWN at him and the floor, while both neighbours look UP at the skylight = the pitch flipped for one
  pair. Index: same gen, counters unchanged; status: view-edits = 2 x second renders exactly (every eye view edited, none missed).
- Tests that ruled things out: `bo reflect off` 23:58 13 s -> no flash, but the reflection hook changes NOTHING while paused (only a
  counter), so inconclusive; staring still at the skylight 00:01 13 s -> no flash (max 1.8) => not the skylight / a reflection
  render; flashes seem tied to head MOVEMENT. First person off, camera position 0 (not CAMPOS / FP).
- History: Origins map 2026-10-13 "occasional full field of view blackout for one frame and also occasionally recenters" (MAPMONO
  guess) = probably the same bug, so it predates today's STEREOPOSE change.
FLIPWATCH (built, installs at game exit): per eye render, the game camera's, the head's and the final view's pitch; any > 25 deg jump
from that eye's previous render is logged with both frames' values (80 lines max). Next run: pause + head movement ~30 s; the lines say
whether the game camera, the head pose or our view math flips.

### FLIPWATCH RESULTS run 00:08-00:12 (JJ: dirt overlay gone; "the pause menu is flickering heavily" - existed before PAUSEDIRT;
### "I honestly can't tell if it's a camera jump, but it just feels like black frames"; NEW: black square entering gameplay from load)
- PAUSEDIRT HEADSET: dirt gone (JJ). 3454 grime passes drawn with white in ~7 s.
- FLIPWATCH LIVE: only the GAME camera jumps (head pitch steady -3..-5 / 2.2): 00:09:06.27-07.80 and 00:12:25.79+ = the ~1.5 s
  AFTER the pause menu CLOSES (MENUSTATE Close 00:09:06.240, 00:12:25.185). Base pitch per pair alternates 79.7 / -71.4 / 70.1 /
  -66 / 19.4 / 34.1 ... (-71.4 and 79.7 recur = fixed cameras), sometimes TWO views in one generation (gen 4333, 4337). No jumps
  while the pause menu is open (08:53-09:06, 11:48-12:25).
- EYEBURST 00:11:47 + 00:12:08 (2 x 20 s, pause then close): open pause = 5 frames scoring 2-4.6 at ~3 s in = the game's own grey
  pause effect animating (bright, not a camera change, flash_480); after the close = 19 flash frames scoring up to 10.6 at the same
  gens as FLIPWATCH (flash_2053: alternating cameras on Batman) = JJ's "black frames".
FLIPWATCH2 + LOADBURST (built, install at game exit): FLIPWATCH logs yaw, camera position, views per generation and the identities
(draw root object, family a1, view state a2, the view) on any jump / 3rd view in a generation (300 lines); LOADBURST records 900
presents (~8 s, both eyes + HUD sheet) once, when a 3D scene comes back after >= 3 s without one. Disk: one 2400-present burst =
~1 GB; today's 8 bursts' raw data deleted after analysis (7.8 GB; flash PNGs and flashes.txt kept).
Next run: load save (LOADBURST catches the black square), pause 5 s, close, play 5 s, pause, close.

### FLIPWATCH2 / LOADBURST RESULTS run 00:17-00:20 (JJ: "Lots of flickering again"; asks if it is the old map flicker - JJ:
### "The map flicker was flickering a skewed copy of part of the map into the area outside the map window" = a different mechanism)
- FLIPWATCH2: 300 lines, 286 of them "view 3/4 of this gen" (two stereo pairs with the SAME pose generation, camera steady) after the
  pause close 00:18:34-00:18:43; pacetrace shows 84-86 pairs/s then (90 in pause), 51-60 pairs/s after the load (00:18:04-09) =
  the game falls just under 90 Hz and now and then renders a second pair before the next locate (pose reused, ~11 ms stale).
  Only 14 real jumps, all at scene cuts (title -> main menu 00:17:47 / 00:17:56, load 00:18:00 / 00:18:03). The post-unpause camera
  alternation (00:09:06, 00:12:25) did NOT happen this run = intermittent.
- LOADBURST 00:18:03-00:18:11 (load end + first pause opening): eye pictures clean (max score 2.1), finished headset images clean ->
  the black square is NOT in the eye pictures = on the HUD panel. Matches City FADEHOLD (the game's fade-in from black drawn on the HUD
  stage = an opaque black box in the room); Origins has gfx_hud FADEHOLD but scene.cpp sets set_fade_hold(0) ("City-specific ... not
  Origins evidence"). eyeburst HUD files are RGB (alpha dropped), so they cannot show an opaque-black panel; a panel capture with
  alpha (City debug_panelseq_after_movie / gfx_hud::save_panel_picture) is needed.
Two launches without a fix -> look up again before the next build (SESSION_START §1): City FADEHOLD / MENUFADE notes, playbook
failure atlas "single-frame wrong view" / "stale pose", Origins MAPMONO history.
Looked up again: playbook b955f224 failure-atlas FAIL-STR-017 ("world lurches once per transition ... one frame rendered from a stale
or wrong viewpoint") -> pattern-catalog STR-010 "drop a bad frame instead of submitting it" (bounded and counted; a suppression that
fires constantly masks another bug); City BATMAN_CITY_PLAN FADEHOLD (black box entering gameplay = the game's fade-in from black on
the HUD stage; panel held while 5 probe points are dark + covered, <= 3 s after a movie / load). JJ: the old map flicker was a
skewed copy of part of the map OUTSIDE the map window = a different mechanism (HUD / menu layer), not this one.
CAMHOLD (built, installs at exit; `bo camhold on|off`, default on): while a pause-type menu is up or < 3 s after a menu closes, a
pair whose game camera jumps > 25 deg / 150 UU from the last good one is drawn from the last good camera; a new camera holding 3
pairs in a row = a real cut, taken. Status: "CAMHOLD pairs held N, real cuts taken N". (Applies STR-010 by redrawing from the good
camera rather than not submitting - no runtime change, the frame stays paced.)
FADEHOLD ON for Origins (`bo fadehold 0|1`): triggered by "3D scene back after >= 3 s without one" (load / video end); log
"gfx_hud: fade-in box gone (...) - panel shown again after N ms" proves it fired. If no box is seen within 0.7 s the panel returns.
Next run: load save (watch for the black square), pause, close, play 5 s, pause, close (watch for flicker). Judge: JJ + log lines.
CAMHOLD2 (JJ: "What about the flicker when entering the pause menu?"): no jump was ever logged while a pause menu opened or was up,
but the 23:56 flashes predate FLIPWATCH, so CAMHOLD is now ALWAYS on (not only around menus); each hold logs the menu context
(pause-type up / screen menu / none, ms since the last open and close; first 60). Captures folder cleared 2026-10-26 (22.5 GB;
today's flash PNGs + flashes.txt kept) - JJ rule: clear recordings once analysed (memory clear-recordings-when-done).

## FASTTRAVEL - black headset through the fast-travel cutscene (JJ 2026-10-26: "I went to fast travel and it played a cutscene but I saw nothing in the headset it was just black")

Looked up: this file EYEBATS / EB1 and the O1 bats notes ("cover -> black hold -> reveal; handoff uses the actual per-present
projection verdict after cinematic fallback ... 60-second watchdog"); memory origins-eyebats-bootblack; menu_state.cpp MS2 note
(the full-screen-movie flag is TRUE on loading screens too, so it cannot tell a cinematic from a loading screen); hud_capture
screen-only detector. No fast-travel notes existed.
LIVE (batman_origins.log 23:20:59-23:21:26, STEREOPOSE build, no-pose=0): map closed -> bats event #3 "bats -> black" ->
IsPlayingFullScreenMovie TRUE -> letterbox ON (engine cinematic bars) -> "cinematic quad ON (strict=1 stale=1 screenOnly=0)" ->
nothing until JJ quit; stereo pairs and DLSS kept running. Cause (SOURCE wipe.cpp): the Hold phase reveals only after 350 ms of
headset PROJECTION; an in-engine cinematic whose camera bypasses CalcView goes to the cinematic screen (projection false), so the
black covered it for the whole cutscene (60 s watchdog).
Fix: hud::world_pass() (scene vote leader present, 3-interval hysteresis, beside screen_only); transitions paint_eye treats "cinematic
screen + world pass" like projection for the bat timeline -> reveal onto the cinematic screen, logged "[bo bats] reveal onto the
cinematic screen". Loading / FMV screens have no world pass and stay black as designed. Built (4/4), INSTALLED 2026-10-26
(sha256 6E3CD4A2..., manifest re-checked; previous in backups/pre-FASTTRAVEL-2026-10-26). HEADSET pending.
Test: fast travel twice (and one normal Continue load): the cutscene shows after the bats; loading screens still black; log line above.
FASTTRAVEL2 (JJ 23:26 "Still not seeing the cutscene, it's all black"): LIVE log 23:26:19-35 - same sequence, NO reveal line, bats
phase=3 (Hold) at 23:26:34. Likely cause: world_pass needed 3 presents in a row, but in the cinematic presents and world passes
alternate (4106 presents / 2637 draw roots in that window), so it never latched (not proven: no log of it). Now: world pass seen
within 250 ms, edges logged "[hud] world pass ON/off", plus one "[bo bats] black hold over the cinematic screen: 3D scene seen /
NOT seen" line per hold. Installed 2026-10-26 (sha256 A15E6540...). If it fails again the log names the half that failed.
FASTTRAVEL3 (JJ 23:31 "It's still not working"; looked up again: the two attempts assumed a 3D cinematic - wrong). LIVE 23:31:01:
letterbox ON 1.485, "world pass off" 1.618, cinematic quad ON 1.692, "3D scene NOT seen -> stays black" = the fast-travel cutscene
is a full-screen VIDEO (IsPlayingFullScreenMovie TRUE), no world pass. The EYEBATS design held black over FMV / loading on purpose;
the flat game shows this video, so any real picture on the cinematic screen now ends the hold: world pass OR the pixel watch's
letterbox (bars only around picture content; all-black reads "top 1420" = no bars). Expect the short video after Continue
(23:30:35-38, also letterboxed) to show too. Log: "[bo bats] reveal onto the cinematic screen (letterboxed video / cinematic)".
Installed 2026-10-26 (sha256 90BE3CB1..., manifest re-checked).
FASTTRAVEL3 RESULT (JJ 23:33 "still not showing the pre-render video" + "I'm not seeing any bats either"): LIVE 23:33:33.450 "reveal
onto the cinematic screen" = the hold DID lift, yet still black, and no bats at all -> the problem is upstream of the bats timeline
(what the headset is sent), not the hold. Not the "all-black frame -> no layers" rule (never logged), not submission idle (no
heartbeat), room screen live since boot.
FTPROBE (JJ: "Do you need a test probe or something to capture what's going on at that point?"): armed once per launch by a bats
event while the map is open (= fast travel); 3 automatic shots at +0.4 s (bats over the map), +2 s and +5 s (video): game_<eye|flat>.bmp
= the backbuffer as handed over, eye_left / eye_right.bmp = exactly what each eye was sent (room screen path or projection path),
and a "[probe] shot n" log line: layer sent (NOTHING / eye images / floating quad), layers, projection, cinematic, room screen,
shouldRender, skipMono (map mono-skip), letterbox, world pass, screen-only, bats-in-eye paint count. In
%LOCALAPPDATA%\Batman_Origins\captures\probe_<tick>\shot0..2 (~160 MB). Installed 2026-10-26 (sha256 011439A1...).
Candidates it separates: (a) frames not captured at all (skipMono / shouldRender) = headset holds an old black image; (b) room
screen draws a black copy (game_flat.bmp has the video but eye images black = copy/source problem); (c) game_flat.bmp itself black
(the video is not in the backbuffer we copy); (d) floating quad sent instead of eye images (no bats possible on a quad).
FTPROBE RESULT (run 23:40-23:41, JJ: "The bat and the video actually worked that time"): HEADSET - fast-travel bats + video show.
Probe: shot0 (bats over the map) eye images sent, projection; shots 1-2 (video) eye images via the room screen, letterbox 337/337,
world pass 0. (Why the 23:33 run looked black is unproven: same reveal at the same moment; JJ quit 11 s in.) Probe now on request
only: `bo bats probe` arms it for the next fast travel.
BATSFLOW (JJ: at the video's end "bats then played across the video and then faded to black and then bats played again across
full field of view before the game came in ... not very clean"): LIVE 23:41:34.05 video ends, .075 projection back, .13 bats
event #4 - the picture was ready BEFORE the bats, yet Hold waited cover + 350 ms (the settle counted from the end of the cover).
wipe.cpp: the 350 ms settle now counts from when the picture came back, so an already-steady picture is revealed the moment the
cover ends = one bats-in / bats-out. Real loads (no projection) still hold black. Installed 2026-10-26 (sha256 18DB176C...).

## STEREOPOSE - flat-frame fix ported from Asylum (Godoy301's PR #1, HEAD-AIM4; JJ 2026-10-25: "incorporate these ... into the other 3 too")

Looked up: Asylum BUILD_PLAN "HEADAIM" (the PR and its evidence); this game's VIEWDEBOUNCE3 (scene.cpp: "the gameplay caller itself
can be ineligible for one frame (no fresh pose generation) ... that frame renders mono") = the same cause; VR_ESSENTIALS §4, playbook
09 #pose-timing-contract, STR-002. City and Knight do NOT have this bug (City builds both eyes from one head read; Knight holds the last
head pose), and all three already have a world-scale slider. Head aim is not ported yet (needs each game's own Controller.Rotation
offset and HUD path; after JJ tries it in Asylum).

Cause (SOURCE): openxr_runtime published the head pose + generation before xrLocateViews; draw_detour read the head, then that
generation's eyes from the pose ring under a second lock. A read between the two found no eyes -> poseValid false -> mono frame.
Fix: core/vr/stereo_pose_mailbox.h (verbatim from Asylum) published after both locates and the ring; draw_detour takes head, eyes,
FOVs and generation from ONE read (vr::get_stereo_pose). Tracking loss and teardown still invalidate it. New status count `no-pose`
(draw roots without a complete sample). VIEWDEBOUNCE3 stays (it also covers foreign draw roots).
Built in build-stereopose (Release, no warnings), 4/4 checks (build includes CAMPOS). INSTALLED 2026-10-25 (JJ "Yes, install"):
batman_origins.dll sha256 9BC78D19..., originsvr_manifest.json updated and every listed file re-checked; previous CAMPOS DLL +
manifest in backups/pre-STEREOPOSE-2026-10-25. NOT released.

### TEST PLAN STEREOPOSE (JJ drives, one launch; CAMPOS tests can share it)
| # | what JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | 5 min gameplay, head moving, through snow / smoke and a map open-close | no flat frames | none seen; `[bo] status ... no-pose` small, and the VIEWDEBOUNCE "camera mode off -> ON" blips gone from the log |
| 2 | Meta-button recentre, take the headset off and on | tracking loss / recovery | picture returns normally, no stuck flat view |

## CAMPOS - adjustable third-person camera position from Arkham Knight (JJ 2026-10-25: "put the adjustable camera from our Arkham knight mod panel into the other 3 arkham mods")

Looked up: AKVR TPOFFSET (akvr/src/camera.cpp TPOFFSET, hooks.cpp VIEW "camera position"; akvr_native/NOTES.md "TPPITCH + TPOFFSET
2026-10-18", released in AK 0.3.1); this game's FIRSTPERSON / FPGATE (the camera mover this rides on); memory
arkham-first-person-asylum-origins, batman-city-first-person, origins-install-manifest. Playbook / framework: nothing new needed - a
translation of the head-free game camera at the scene-view inputs, the engine rebuilds culling from it (same route as first person).
AK's TPPITCH look-down limit was NOT ported (it is a separate AK-only control; JJ asked for the camera position).

What it does (SOURCE): panel VIEW -> "camera position  (third person)" (open the first time): left / right, down / up, back / forward,
-300..+300 cm each, "Reset position". The game camera's origin moves along its LEVEL right / LEVEL forward and world up (AK's frame,
so it never swings with the head or tilts with the camera's pitch). Applied to the head-free game camera before the eye offsets, so
both eyes, markers and AFW follow it. Only in third-person gameplay: off in first person and through first person's FPGATE checks
(menus, no Batman found, game camera more than 12 m from Batman = main menu stand-in, distant cutscenes). Costs
nothing while all three are 0 (default). The game's own camera collision does not know about the shift: big values can put the
camera into walls (same as AK).
Saved: settings ini [camera_position] right_cm / up_cm / forward_cm (first_person.cpp save/install). Command: `bo fp cam R U F` (cm). Status: "moved frames N" under TESTS.

Built (Release, no warnings), offline checks pass; installed batman_origins.dll into JJ's game folder 2026-10-25 (previous DLL + manifest in
backups/pre-CAMPOS-2026-10-25; launcher manifest hash updated and re-checked). Not released. HEADSET pending.

### TEST PLAN CAMPOS (JJ drives, one launch, no restart needed)
| # | what JJ does | what it proves | how to judge |
|---|---|---|---|
| 1 | gameplay, panel VIEW -> camera position: left / right to +60 | the shift happens, along the level right | Batman slides to the left of the view, horizon stays level |
| 2 | turn with the stick and with the head | frame is the game camera's heading, not the head | offset stays to the side of Batman on stick turns; no swing on head turns |
| 3 | down / up +50, back / forward -100 | the other two axes | camera higher / further back; looking down does not tilt the shift |
| 4 | pause menu, map, first person on | the gates | game camera unchanged in menus and first person |
| 5 | Reset position | defaults restore | the game's own framing |

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

