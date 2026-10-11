#pragma once
// Origins PERCAPTURE, ported from Batman City (BATMAN_CITY_PLAN.md round 9, JJ "well done, you fixed it").
// The wet ground reads a planar reflection that the engine's reflection probe renders from the view family's
// view, mirrored. The probe asks "update required?" with the family's game time; both eye passes share one
// game time, so it renders for the left eye only and the right eye projects the left eye's reflection
// (skewed, stretched, sliding with head turns). In the right pass, a probe that updated in this frame's left
// pass is told to update again, so it renders from the right eye's own view.
// STATIC (RE_NOTES "Reflection probe"): UpdateRequired RVA 0x3FDB90, vtbl 0x14BA744 [2].
#include <cstdint>
namespace bvr::bo::reflect {
bool install(uintptr_t imageBase);            // validates and queues the hook; caller applies the batch
void set_render_pass(uint32_t pass,int eyeSign); // render thread, BeginDrawing execute
bool command(const char* args);               // "on" | "off" | "status"
void draw_ui();
void status();
}
