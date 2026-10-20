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
// Game thread, the scene view constructor: the game camera's view matrix (row vectors) moved to his eyes.
void apply(float view[4][4], float unitsPerMetre);
bool on();
void set_on(bool on);
void toggle();
void draw_ui();
bool command(const char* args); // after "fp "

} // namespace bvr::bo::first_person
