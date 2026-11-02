#pragma once
#include <cstdint>

// All time comparisons use subtraction so a DWORD tick rollover is harmless.
class DoubleTap {
    bool down_ = false, armed_ = false;
    std::uint32_t first_ = 0;
    int actor_ = -1, weapon_ = -1;
public:
    void reset(bool down = false) { down_ = down; armed_ = false; }
    int update(std::uint32_t now, bool down, bool eligible, int actor,
               int drawnWeapon, std::uint32_t window) {
        if (!eligible || (armed_ && (actor != actor_ || now - first_ > window)))
            armed_ = false;
        const bool pressed = down && !down_;
        down_ = down;
        if (!eligible || !pressed) return -1;
        if (armed_) {
            // Allow the first press to have holstered the original weapon, but
            // do not spin a different revolver selected between the two taps.
            const int result = drawnWeapon == -1 || drawnWeapon == weapon_ ? weapon_ : -1;
            armed_ = false;
            return result;
        }
        if (drawnWeapon >= 4 && drawnWeapon <= 7) {
            armed_ = true; first_ = now; actor_ = actor; weapon_ = drawnWeapon;
        }
        return -1;
    }
};
