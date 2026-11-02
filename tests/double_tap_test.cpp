#include "../src/double_tap.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
int main() {
    DoubleTap t;
    auto step = [&](std::uint32_t ms, bool down, int weapon = 4, int actor = 1, bool ok = true) {
        return t.update(ms, down, ok, actor, weapon, 300);
    };
    assert(step(0, true) == -1);
    assert(step(50, true) == -1); // key repeat / hold is not another tap
    assert(step(80, false, -1) == -1); // ordinary game holster
    assert(step(160, true, -1) == 4); // still remembers original revolver
    assert(step(170, true) == -1); // at most one trigger while held
    t.reset();
    assert(step(0, true) == -1); step(50, false);
    assert(step(301, true) == -1); // expired first tap
    t.reset(); step(0, true); step(50, false);
    assert(step(100, true, 5) == -1); // switched weapons
    t.reset(); step(0, true); step(50, false);
    assert(step(100, true, 8) == -1); // switched to a long gun
    t.reset(); assert(step(0, true, 8) == -1); step(50, false, 8);
    assert(step(100, true, 8) == -1); // long guns cannot arm
    t.reset(); step(0, true); step(50, false);
    assert(step(100, true, 4, 2) == -1); // respawn / actor change
    t.reset(); step(0, true); step(50, false, 4, 1, false);
    assert(step(100, true) == -1); // pause invalidates pending tap
    t.reset(); assert(step(0, true, -1) == -1); step(50, false, -1);
    assert(step(100, true, -1) == -1); // holstered / long gun can't arm
    t.reset(); step(UINT32_MAX - 100, true); step(UINT32_MAX - 50, false);
    assert(step(50, true) == 4); // tick rollover
    t.reset(); step(0, true); step(50, false);
    assert(step(300, true) == 4); // inclusive window edge
    t.reset(true);
    assert(step(0, true) == -1); // block rearming until release after animation
    step(50, false); step(100, true); step(150, false);
    assert(step(200, true) == 4);
    std::puts("PASS: held input, double tap after holster, timeout, weapon change, actor change, pause, empty hand, timer rollover, window boundary, rearm");
}
