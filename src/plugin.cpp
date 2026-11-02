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

#include "twirl_session.hpp"

static HMODULE self;
static char iniPath[MAX_PATH], logPath[MAX_PATH];
static char inputAction[96] = "@RADIAL_MENU.SHOW_HIDE";
static unsigned windowMs = 300;
static bool enabled = true;
static DoubleTap taps;
static int previousActor = -1, previousWeapon = -1;
static void log(const char* format, ...) {
    FILE* f = std::fopen(logPath, "ab"); if (!f) return;
    std::fprintf(f, "[%lu] ", static_cast<unsigned long>(GetTickCount()));
    va_list args; va_start(args, format); std::vfprintf(f, format, args); va_end(args);
    std::fputs("\r\n", f); std::fclose(f);
}
static bool valid(int actor) { return actor != -1 && Hook::invoke<int>(0xBA6C3E92, actor) != 0; }
static int weaponInHand(int actor) {
    return Hook::invoke<int>(0xAB5FB5AC, actor) ? Hook::invoke<int>(0xA4B2016D, actor) : -1;
}
static bool safeToStart(int actor) {
    return valid(actor) && Hook::invoke<int>(0x2F232639, actor)
        && !Hook::invoke<int>(0x57246C02)
        && Hook::invoke<int>(0x9613C2D0, Hook::invoke<int>(0xAD68A22E))
        && !Hook::invoke<int>(0xF270EAC1, actor)
        && !Hook::invoke<int>(0x12325AE7, actor)
        && !Hook::invoke<int>(0x3918D335, actor)
        && !Hook::invoke<int>(0x39C518DB, actor)
        && !Hook::invoke<int>(0x4B441DC4, actor)
        && !Hook::invoke<int>(0xD39C4A9E, actor)
        && !Hook::invoke<int>(0x7D65D9C7, actor)
        && !Hook::invoke<int>(0x062C5047, "@GENERIC.TARGET", 0, 0);
}
struct Game {
    int lastObservedWeapon = -999;
    bool validActor(int a) { return valid(a); }
    void requestResources() {
        lastObservedWeapon = -999;
        Hook::invoke(0x2988B3FC, "holster_spin", 0);
        Hook::invoke(0xB3039DB7, "custom/holster_spin");
    }
    bool loaded() {
        return Hook::invoke<int>(0x4FFF397D, "holster_spin") &&
               Hook::invoke<int>(0xEEECD85E, "custom/holster_spin");
    }
    void equip(int actor, int weapon) {
        log("Restoring weapon in hand: actor=%d weapon=%d", actor, weapon);
        Hook::invoke(0x8F4B473D, actor, weapon, 1);
    }
    bool drawn(int actor, int weapon) {
        const int observed = weaponInHand(actor);
        if (observed != lastObservedWeapon) {
            log("Preparation weapon: expected=%d observed=%d", weapon, observed);
            lastObservedWeapon = observed;
        }
        return observed == weapon;
    }
    void ready(int player, bool on) { Hook::invoke(0xD0E08B5E, player, static_cast<int>(on)); }
    bool applySet(int actor) { return Hook::invoke<int>(0x39C1E1C0, actor, "holster_spin", 1) != 0; }
    int openSequence() { return Hook::invoke<int>(0x8DA34524); }
    void addAction() { Hook::invoke(0xE32F09B3, 0, "holster_spin"); }
    void closeSequence() { Hook::invoke(0x9EE3053B); }
    void perform(int actor, int seq) { Hook::invoke(0x2DF2298C, actor, seq); }
    void release(int seq) { Hook::invoke(0xB2D35E22, seq, 1); }
    int status(int actor) {
        const int result = Hook::invoke<int>(0x016C6801, actor, 0);
        static int previous = -999;
        if (result != previous) { log("Action task status=%d", result); previous = result; }
        return result;
    }
    void clear(int actor) { Hook::invoke(0x16876A25, actor); }
    void resetTree(int actor) { Hook::invoke(0x07EC142B, actor); }
    void resetSet(int actor) { Hook::invoke(0x7A6C5C2F, actor, 0); }
    void message(const char* text) { log("%s", text); }
};
static Game game;
static TwirlSession<Game> session(game);
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
    GetPrivateProfileStringA("General", "InputAction", "@RADIAL_MENU.SHOW_HIDE", inputAction, sizeof(inputAction), iniPath);
    if (!inputAction[0]) enabled = false;
}
static void ScriptMain() {
    settings(); log("TwirlingGun 0.3 weapon-handoff candidate loaded; enabled=%d", enabled);
    bool emergencyWasDown = false;
    while (true) {
        Hook::wait(0);
        const int actor = Hook::invoke<int>(0xE8CFDD53, -1);
        const DWORD now = GetTickCount();
        const bool down = enabled && Hook::invoke<int>(0x062C5047, static_cast<const char*>(inputAction), 0, 0);
        const bool emergencyDown = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
        const bool emergency = emergencyDown && !emergencyWasDown;
        emergencyWasDown = emergencyDown;
        if (session.active()) {
            // A scripted task may temporarily make IS_PLAYER_CONTROLLABLE false.
            // Never use that predicate to abort our own active action.
            const bool cancel = emergency || !valid(actor) ||
                !Hook::invoke<int>(0x2F232639, actor) || Hook::invoke<int>(0x57246C02) ||
                Hook::invoke<int>(0x3918D335, actor) || Hook::invoke<int>(0xF270EAC1, actor) ||
                Hook::invoke<int>(0x12325AE7, actor);
            session.tick(actor, now, cancel, down);
            taps.reset(down); previousActor = previousWeapon = -1;
            continue;
        }
        if (!enabled || emergency || !safeToStart(actor)) {
            taps.reset(down); previousActor = previousWeapon = -1; continue;
        }
        const int weapon = weaponInHand(actor);
        const int captured = weapon != -1 ? weapon : (actor == previousActor ? previousWeapon : -1);
        const int trigger = taps.update(now, down, true, actor, captured, windowMs);
        if (trigger >= 0) {
            log("Double tap: actor=%d weapon=%d", actor, trigger);
            session.begin(actor, trigger, Hook::invoke<int>(0xAD68A22E), now);
        }
        previousActor = actor; previousWeapon = weapon;
    }
}
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        self = module; DisableThreadLibraryCalls(module); paths();
        if (!Hook::load()) { log("ERROR: ScriptHookRDR exports unavailable"); return FALSE; }
        Hook::reg(module, ScriptMain);
    } else if (reason == DLL_PROCESS_DETACH && Hook::unreg) {
        Hook::unreg(module);
    }
    return TRUE;
}
