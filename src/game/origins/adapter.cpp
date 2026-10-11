#include "game/igame_adapter.h"
#include "scene.h"
#include "window.h"
#include "startup.h"
#include "dlss_bridge.h"
#include "core/framework/command.h"
#include "core/input/dinput_block.h"
#include <cstring>
namespace bvr::bo {
class OriginsAdapter final : public bvr::game::IGameAdapter {
public:
    uint32_t capabilities() const override {
        return scene::hook_live() ? bvr::game::CAP_CAMERA_OVERRIDE | bvr::game::CAP_SCENE_REENTRY : 0;
    }
    bool init(const bvr::pattern_scan::ProcessImage&) override {
        command::enable_present_pump();
        dlss_bridge::install_early();
        window::install();
        startup::install();
        input::dinput_block::install();
        return scene::install();
    }
    void setFov(float) override {}
    void drawDebugUi() override { scene::draw_ui(); }
    bool handleCommand(const char* cmd,const char* args) override {
        return _stricmp(cmd,"bo")==0 && scene::handle_command(args);
    }
};
bvr::game::IGameAdapter* create_adapter() { static OriginsAdapter adapter; return &adapter; }
}
