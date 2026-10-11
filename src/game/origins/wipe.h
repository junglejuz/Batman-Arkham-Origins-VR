#pragma once
#include <cstdint>
#include <vector>
#include <cstddef>
namespace bvr::bo::wipe {
struct Recording {
    unsigned width=0,height=0;
    std::vector<uint32_t> times;
    std::vector<size_t> offsets,lengths;
    std::vector<uint8_t> bytes;
    bool decode_frame(unsigned index,std::vector<uint8_t>& pixels) const;
};
bool decode(std::vector<uint8_t> bytes,Recording&);
enum class Phase { AwaitMenu,Reveal,Cover,Hold,Idle };
enum class Visual { None,Black,Animation };
struct Step {Visual visual=Visual::None;bool invert=false;uint64_t elapsed=0;};
struct Playback {
    Phase phase=Phase::AwaitMenu;
    bool first=true,blackPending=false,beginPending=false;
    uint64_t start=0,holdStart=0,projectionSince=0;
    void event(uint64_t now);
    Step tick(uint64_t now,bool projection,unsigned duration);
    bool owns() const;
};
}
