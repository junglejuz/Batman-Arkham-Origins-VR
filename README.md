<p align="center">
  <img src="https://cdn.cloudflare.steamstatic.com/steam/apps/209000/capsule_616x353.jpg" width="616"
       alt="Batman: Arkham Origins">
</p>

# Batman Origins VR

Batman: Arkham Origins in PC VR, with per-eye DLSS, an AFW speed mode, and OpenXR for Virtual Desktop and SteamVR. The
game draws both eyes itself, every frame, from the same moment (native stereo), so depth is correct and nothing lags
behind when the camera swings. Made the same way as [Batman City VR](https://github.com/junglejuz/Batman-Arkham-City-VR).

## What you get

- **Native stereo 3D** with full head tracking (look around and lean in), on the game's own third-person camera.
- **DLSS for each eye** (NVIDIA RTX graphics cards): each eye is drawn smaller and upscaled by NVIDIA's DLSS, which
  also gives clean anti-aliasing. Choose the quality in the VR menu.
- **AFW (alternate frame warping)**, a choice in the VR menu beside full native stereo: the game draws one eye per
  frame and the other eye is built from that same frame, so both eyes still show the same moment. More frames per
  second on slower PCs.
- **OpenXR, with Virtual Desktop and SteamVR**: runs on your PC VR software's own OpenXR runtime (no extra layers or
  injectors). A separate SteamVR shortcut starts the game through SteamVR without changing your PC's VR settings.
- **The render size comes from your headset**: the launcher asks your PC VR software for its picture size each time.
- **HUD on a panel in front of you**, at a depth both eyes agree on.
- **Menus, the map and loading screens** on screens in the room, with full-view bat transitions.
- **Puddle reflections drawn for each eye**, so wet streets look right in both eyes.
- **An in-headset VR menu**: hold both thumbsticks down (or press F10). World scale, HUD size and distance, render
  size, DLSS, and more. Settings are saved.

## What you need

- **Batman: Arkham Origins** on Steam, on Windows 10 or 11.
- **A PC VR headset and the software you use for PC VR games** (for example Virtual Desktop, SteamVR or the Meta Quest
  Link app).
- **A gamepad.** Quest Touch controllers work through Virtual Desktop, which presents them as a gamepad.

Tested with: Quest 3 through Virtual Desktop, RTX 4070 Ti. A SteamVR launcher is included; it has not been tested with
this game yet.

## Installing

1. Start the game once from Steam as normal, wait for the main menu, and quit (this makes the game's settings files).
2. Download `BatmanOriginsVR-ArkhamOrigins.zip` from [Releases](../../releases) and unpack it anywhere (7-Zip works).
   It is about 60 MB, most of it NVIDIA's DLSS library.
3. Double-click `Install-BatmanOriginsVR.bat`. If Windows SmartScreen appears: More info, then Run anyway. The
   installer puts everything into the game folder; you don't copy anything there yourself.
4. Start your PC VR software and make sure it is the active OpenXR runtime (Virtual Desktop and SteamVR each have a
   setting for this).
5. Start the game with the **Batman Origins VR** shortcut on your desktop. To play through SteamVR instead, use
   **Batman Origins VR (SteamVR)**.

The installer turns depth of field, motion blur and V-sync off, and saves your own settings first. Each VR start sets
the game to a window at your headset's picture size and puts your own graphics settings back when the game closes, so
starting the game from Steam still plays the normal flat game.

## In the game

| | |
|---|---|
| VR menu | hold both thumbsticks down for half a second, or F10 |
| Recenter | hold the headset's menu / Meta button, or F12 |
| Skip the opening logos | any face button, Start or Back |
| Save a picture of both eyes | the VR menu's capture button, or F2 |

## If something is wrong

| What you see | What to do |
|---|---|
| Nothing in the headset, the game plays on the monitor | Start your PC VR software before the game, make it the active OpenXR runtime, and start the game from the Batman Origins VR shortcut. |
| A window says the headset could not be asked for its picture size | Connect the headset to your PC VR software first, then start the shortcut again. |
| The installer stops: "the game has not made its settings yet" | Start the game once from Steam, wait for the main menu, quit, run the installer again. |
| Pimax (or other VR software with no 32-bit OpenXR runtime): nothing in the headset | The shortcut then uses SteamVR's 32-bit runtime by itself: install SteamVR. Do not run Steam or the game as administrator. |
| Low frame rate | VR menu: lower the picture size (VR resolution), or turn on DLSS or AFW. |
| The world feels too big or too small | VR menu: world scale. |

## Removing it

Run **Uninstall Batman Origins VR** from the Start menu (or `Uninstall-BatmanOriginsVR.bat` in the game folder). It
removes the mod and the shortcuts, puts your game settings back as they were before you installed it, and deletes the
mod's own VR settings and logs (in `%LOCALAPPDATA%\Batman_Origins`).

## Building from source

Visual Studio 2022 and CMake:

```
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
cmake -S helper64 -B build64 -G "Visual Studio 17 2022" -A x64
cmake --build build64 --config Release
ctest --test-dir build -C Release
python tools/make_package.py
```

The DLSS helper needs NVIDIA's NGX SDK; the bat transition recording is not in this repository (it comes with the
release). Development notes: `BUILD_PLAN.md`, `RE_NOTES.md`, `FIX_CHANGES.md`.

## Credits

Built on [bioshock-vr](https://github.com/mohamad-balouza/bioshock-vr) (MIT), MinHook, Dear ImGui and the OpenXR SDK.
The per-eye DLSS method follows the [Luma Framework](https://github.com/Filoppi/Luma-Framework)'s Arkham Knight mod.
DLSS is NVIDIA's. Full list: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
The picture at the top is the game's own Steam store artwork, shown from Steam (Batman: Arkham Origins © Warner Bros.
Entertainment); it is not part of this repository.

Batman: Arkham Origins © Warner Bros. Entertainment Inc. Batman and related characters are trademarks of DC Comics. This
is an unofficial fan mod and needs your own copy of the game.
