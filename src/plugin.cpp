#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <type_traits>
#include "double_tap.hpp"

// ScriptHookRDR 1.5.1 exports MSVC C++ names. Resolve the published ABI
// explicitly so this plugin can also be built with MinGW; no guessed addresses.
namespace Hook {
    void (*wait)(DWORD) = nullptr;
    void (*reg)(HMODULE, void(*)()) = nullptr;
    void (*unreg)(HMODULE) = nullptr;
    void (*init)(std::uint64_t) = nullptr;
    void (*push)(std::uint64_t) = nullptr;
    std::uint64_t* (*call)() = nullptr;
    template<class T> bool bind(HMODULE h, T& fn, const char* name) {
        const auto p = GetProcAddress(h, name);
        static_assert(sizeof(p) == sizeof(fn));
        std::memcpy(&fn, &p, sizeof(fn));
        return fn != nullptr;
    }
    bool load() {
        auto h = GetModuleHandleA("ScriptHookRDR.dll");
        return h && bind(h, wait, "?scriptWait@@YAXK@Z")
            && bind(h, reg, "?scriptRegister@@YAXPEAUHINSTANCE__@@P6AXXZ@Z")
            && bind(h, unreg, "?scriptUnregister@@YAXPEAUHINSTANCE__@@@Z")
            && bind(h, init, "?nativeInit@@YAX_K@Z")
            && bind(h, push, "?nativePush64@@YAX_K@Z")
            && bind(h, call, "?nativeCall@@YAPEA_KXZ");
    }
    template<class T> void arg(T value) {
        static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= 8);
        std::uint64_t slot = 0;
        std::memcpy(&slot, &value, sizeof(T));
        push(slot);
    }
    template<class R = void, class... A> R invoke(std::uint32_t hash, A... args) {
        init(hash); (arg(args), ...);
        auto result = call();
        if constexpr (!std::is_void_v<R>) {
            R value{}; if (result) std::memcpy(&value, result, sizeof(R));
            return value;
        }
    }
}

static HMODULE self;
static char iniPath[MAX_PATH], logPath[MAX_PATH];
static char animSet[64] = "holster_spin", animClip[64] = "holster_spin";
static char inputAction[96] = "@RADIAL_MENU.SHOW_HIDE";
static unsigned windowMs = 300;
static bool enabled = true;
static DoubleTap taps;
static int previousActor = -1, previousWeapon = -1;
static int activeActor = -1, activeWeapon = -1;
static bool changedSet = false, sawAnimation = false, setRequested = false;
static DWORD phaseAt = 0;
enum class Phase { Idle, Loading, Equip, Starting, Playing };
static Phase phase = Phase::Idle;

static void log(const char* format, ...) {
    FILE* f = std::fopen(logPath, "ab"); if (!f) return;
    std::fprintf(f, "[%lu] ", static_cast<unsigned long>(GetTickCount()));
    va_list args; va_start(args, format); std::vfprintf(f, format, args); va_end(args);
    std::fputs("\r\n", f); std::fclose(f);
}
static void paths() {
    char path[MAX_PATH]{};
    GetModuleFileNameA(self, path, MAX_PATH);
    char* slash = std::strrchr(path, '\\'); if (slash) slash[1] = '\0'; else path[0] = '\0';
    std::snprintf(iniPath, sizeof(iniPath), "%sTwirlingGun.ini", path);
    std::snprintf(logPath, sizeof(logPath), "%sTwirlingGun.log", path);
}
static void settings() {
    enabled = GetPrivateProfileIntA("General", "Enabled", 1, iniPath) != 0;
    windowMs = GetPrivateProfileIntA("General", "DoubleTapMs", 300, iniPath);
    if (windowMs < 100 || windowMs > 600) windowMs = 300;
    GetPrivateProfileStringA("Animation", "Set", "holster_spin", animSet, sizeof(animSet), iniPath);
    GetPrivateProfileStringA("Animation", "Clip", "holster_spin", animClip, sizeof(animClip), iniPath);
    GetPrivateProfileStringA("General", "InputAction", "@RADIAL_MENU.SHOW_HIDE", inputAction, sizeof(inputAction), iniPath);
    if (!animSet[0] || !animClip[0] || !inputAction[0]) enabled = false;
}
static bool valid(int actor) { return actor != -1 && Hook::invoke<int>(0xBA6C3E92, actor) != 0; }
static bool playing(int actor) { return Hook::invoke<int>(0x1ADE21EB, actor, static_cast<const char*>(animClip)) != 0; }
static int weaponInHand(int actor) {
    if (Hook::invoke<int>(0x6426CCD6, actor)) return -1;
    return Hook::invoke<int>(0xA4B2016D, actor);
}
static bool safe(int actor) {
    return valid(actor) && Hook::invoke<int>(0x2F232639, actor)
        && !Hook::invoke<int>(0x57246C02)
        && Hook::invoke<int>(0x9613C2D0, Hook::invoke<int>(0xAD68A22E))
        && !Hook::invoke<int>(0xA3AB3708, actor)
        && !Hook::invoke<int>(0x3918D335, actor)
        && !Hook::invoke<int>(0x39C518DB, actor)
        && !Hook::invoke<int>(0x4B441DC4, actor)
        && !Hook::invoke<int>(0xD39C4A9E, actor)
        && !Hook::invoke<int>(0x7D65D9C7, actor)
        && !Hook::invoke<int>(0x062C5047, "@GENERIC.TARGET", 0, 0);
}
static void finish(bool holster, bool clearOwnTask) {
    if (valid(activeActor)) {
        // Only clear a task when the exact animation started by us is playing.
        if (clearOwnTask && sawAnimation && playing(activeActor)) Hook::invoke(0x16876A25, activeActor);
        if (changedSet) Hook::invoke(0x7A6C5C2F, activeActor, 0);
        if (holster) Hook::invoke(0xFE9903CC, activeActor, 1);
    }
    changedSet = sawAnimation = false;
    phase = Phase::Idle; activeActor = activeWeapon = -1;
    taps.reset(true); // Require release before another gesture.
}
static void begin(int actor, int weapon, DWORD now) {
    activeActor = actor; activeWeapon = weapon; phaseAt = now;
    if (!setRequested) {
        Hook::invoke(0x2988B3FC, static_cast<const char*>(animSet), 0);
        setRequested = true;
    }
    phase = Phase::Loading;
    log("Double tap: actor=%d weapon=%d; requesting set=%s clip=%s", actor, weapon, animSet, animClip);
}
static void tickAnimation(int actor, DWORD now) {
    if (actor != activeActor || !safe(activeActor)) {
        log("Cancelled: gameplay state changed"); finish(false, true); return;
    }
    if (phase == Phase::Loading) {
        if (Hook::invoke<int>(0x4FFF397D, static_cast<const char*>(animSet))) {
            // TASK_PLAY_ANIM looks up its clip in the actor's animation sets.
            if (!Hook::invoke<int>(0x39C1E1C0, activeActor, static_cast<const char*>(animSet), 0)) {
                log("ERROR: SET_ANIM_SET_FOR_ACTOR rejected set"); finish(false, false); return;
            }
            changedSet = true;
            Hook::invoke(0x8F4B473D, activeActor, activeWeapon, 1);
            phase = Phase::Equip; phaseAt = now;
        } else if (now - phaseAt > 3000) {
            log("ERROR: animation set did not load; verify Animation.Set"); finish(false, false);
        }
    } else if (phase == Phase::Equip) {
        if (now == phaseAt) return;
        // Verified against the supplied EXE's native handler: slots 2/3/4/8
        // are floats; slot 5 is an integer; slots 6/7 are boolean integers.
        // The public SDK incorrectly types slots 5 and 8.
        Hook::invoke(0x5AB552C6, activeActor, static_cast<const char*>(animClip),
                     1.0f, 0.0f, 1.0f, 0, 0, 0, 0.0f);
        phase = Phase::Starting; phaseAt = now;
        log("Animation task submitted");
    } else {
        const bool isPlaying = playing(activeActor);
        if (isPlaying && !sawAnimation) {
            sawAnimation = true; phase = Phase::Playing; phaseAt = now;
            log("Game confirms animation is playing");
        } else if (sawAnimation && !isPlaying) {
            log("Animation ended; holstering"); finish(true, false);
        } else if (!sawAnimation && now - phaseAt > 1200) {
            log("ERROR: game did not confirm clip playback; verify Animation.Clip"); finish(false, false);
        } else if (sawAnimation && now - phaseAt > 6000) {
            log("ERROR: animation exceeded time limit; cancelling"); finish(false, true);
        }
    }
}
static void ScriptMain() {
    settings(); log("TwirlingGun 0.1 experimental loaded; enabled=%d; input=%s", enabled, inputAction);
    while (true) {
        Hook::wait(0);
        const int actor = Hook::invoke<int>(0xE8CFDD53, -1);
        const DWORD now = GetTickCount();
        const bool down = enabled && Hook::invoke<int>(0x062C5047, static_cast<const char*>(inputAction), 0, 0);
        if (phase != Phase::Idle) {
            tickAnimation(actor, now); previousActor = -1; previousWeapon = -1; continue;
        }
        if (!enabled || !safe(actor)) {
            taps.reset(down); previousActor = -1; previousWeapon = -1; continue;
        }
        int weapon = weaponInHand(actor);
        // A normal holster can run before this fiber in the first-press frame.
        // Keep only the immediately preceding frame's drawn revolver.
        const int captured = weapon != -1 ? weapon : (actor == previousActor ? previousWeapon : -1);
        const int trigger = taps.update(now, down, true, actor, captured, windowMs);
        if (trigger >= 0) begin(actor, trigger, now);
        previousActor = actor; previousWeapon = weapon;
    }
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        self = module; DisableThreadLibraryCalls(module); paths();
        if (!Hook::load()) { log("ERROR: ScriptHookRDR 1.5.1 exports unavailable"); return FALSE; }
        Hook::reg(module, ScriptMain);
    } else if (reason == DLL_PROCESS_DETACH && Hook::unreg) {
        Hook::unreg(module);
    }
    return TRUE;
}
