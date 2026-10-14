#pragma once
// MENUSTATE (2026-10-11, JJ: the map copy flickers in City and Origins after four fixes; playbook b955f224 04 "Find the
// engine's authoritative UI state instead of inferring it"). Every map fix so far keyed on a draw-shape guess
// (MAPSCREEN: a big untextured shape), which fired on the menu -> game wipe and never on the Origins map.
//
// Probe stage: the game's own menu natives (Scaleform movie Start / Close / SetPause / focus, the player controller's
// active focus movie, full-screen movie and pause flag) are found BY NAME in the live native table (UE3:
// { const char* "<Class>exec<Func>", impl }), hooked, and every call is logged with the movie's class and object name
// (GNames found live). Getters log only when their answer changes. The log tells which call marks the map, the pause
// menu and their closing; the fixes then key on that instead of draw shapes.

#include <string>

namespace bvr::bo::menu_state {

void install();                              // after the code is plaintext (after the engine hook batch)
void status_line(char* out, unsigned n);     // hooked natives, calls, name table
void dump();                                 // `bo menuprobe`: current focus movie + movies seen

// MS2: the menu state from the game's own calls (pause / map / upgrades: a movie Started with focus, until Closed).
bool live();                                 // Start / Close / SetFocus hooked
bool menu_open();
bool screen_menu_open();                     // MS3: an open menu that is not a pause menu (map / upgrades / database)
unsigned long long menu_open_at();           // GetTickCount64 of the last opening / closing
unsigned long long menu_closed_at();
void clear_stale(const char* why);           // the adapter's safety: the game has run on for a while

} // namespace bvr::bo::menu_state
