#pragma once
// FIRSTPERSON (JJ 2026-10-20: "add the same first person mode we have for the other arkham games ... add it to origins too").
// Batman City's mode (batman_city scene.cpp FIRSTPERSON / HEADCAM / FPGATE) with Origins' own engine offsets (RE_NOTES
// "FIRSTPERSON"): the camera at Batman's eyes, following his head bone, the look direction stays the game camera's.
#include <cstdint>

namespace bvr::bo::first_person {

void install(uintptr_t imageBase, const wchar_t* settingsPath); // settings + both-sticks double click
// Game thread, before the FIRST eye's CalcSceneView of a pair (the second eye reuses it). `blocked` = why the game's own
// camera must stay this frame (menu), or nullptr.
void update(void* localPlayer, const char* blocked);
// Game thread, the scene view constructor: the game camera's view matrix (row vectors) moved to his eyes. HEADAIM: headQ =
// this pair's recentred head orientation (OpenXR x, y, z, w); firstEye = the pair's first view (head aim decided once per pair).
// lastEye = the pair's last view (POVAIM writes the head's look for the next tick's aim only after both eyes have read the camera).
void apply(float view[4][4], float unitsPerMetre, const float headQ[4], bool firstEye, bool lastEye);
// AIMTRIGGER: the left trigger is held (or was within 0.3 s) - HEADAIM and HUDAIM.
bool left_trigger_now();
// LEANBACK: the final eye's world origin (headset movement included); pushed forward along the look if leaning back would bring it
// nearer than the set distance to his head. True = moved.
bool clamp_eye(float origin[3], float unitsPerMetre);
// AIMFIND: 20 s recording of the controller + the objects it points to, with the game camera's yaw / pitch (TESTS button).
void aim_finder_start();
unsigned aim_finder_seconds_left(); // 0 = idle
bool on();
void set_on(bool on);
void toggle();
void draw_ui();
void draw_camera_ui(); // CAMPOS: third-person camera position sliders (VIEW section)
bool command(const char* args); // after "fp "

} // namespace bvr::bo::first_person
