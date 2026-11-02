#pragma once
#include <cstdint>

// Verified Rampage action sequence, with bounded waits and owned-state cleanup.
// Api is injected so failure paths can be exercised without a running game.
template<class Api> class TwirlSession {
    Api& api;
    enum class Phase { Idle, Loading, Equip, Running } phase = Phase::Idle;
    int actor = -1, weapon = -1, player = -1;
    std::uint32_t since = 0;
    bool readyOwned = false, setOwned = false, taskOwned = false;
public:
    explicit TwirlSession(Api& a) : api(a) {}
    bool active() const { return phase != Phase::Idle; }
    void begin(int a, int w, int p, std::uint32_t now) {
        if (active()) return;
        actor = a; weapon = w; player = p; since = now;
        phase = Phase::Loading;
        api.requestResources();
        api.message("Loading holster_spin + custom/holster_spin");
    }
    void finish(bool interrupted) {
        if (!active()) return;
        // Clear only when this session actually submitted its own sequence.
        if (api.validActor(actor)) {
            if (interrupted && taskOwned) { api.clear(actor); api.resetTree(actor); }
            if (setOwned) api.resetSet(actor);
        }
        if (readyOwned) api.ready(player, false);
        readyOwned = setOwned = taskOwned = false;
        phase = Phase::Idle;
    }
    void tick(int currentActor, std::uint32_t now, bool cancel) {
        if (!active()) return;
        if (cancel || currentActor != actor || !api.validActor(actor)) {
            api.message("Cancelled; releasing owned action state"); finish(true); return;
        }
        if (phase == Phase::Loading) {
            if (api.loaded()) {
                if (!api.drawn(actor, weapon)) api.equip(actor, weapon);
                phase = Phase::Equip; since = now;
            } else if (now - since >= 3000) {
                api.message("ERROR: resources timed out; actor was not modified"); finish(false);
            }
        } else if (phase == Phase::Equip) {
            if (now == since) return; // let normal draw/holster input settle
            if (!api.drawn(actor, weapon)) {
                if (now - since >= 1000) {
                    api.message("ERROR: original revolver not drawn"); finish(false);
                }
                return;
            }
            readyOwned = true; api.ready(player, true);
            setOwned = true; // cleanup also if the native partially applies and fails
            if (!api.applySet(actor)) {
                api.message("ERROR: animation set rejected; restoring state"); finish(false); return;
            }
            const int seq = api.openSequence();
            if (seq < 0) {
                api.message("ERROR: sequence allocation failed; restoring state"); finish(false); return;
            }
            api.addAction(); api.closeSequence();
            taskOwned = true; api.perform(actor, seq); api.release(seq);
            phase = Phase::Running; since = now;
            api.message("TASK_ACTION_PERFORM sequence submitted (no TASK_PLAY_ANIM)");
        } else {
            const int state = api.status(actor);
            if (state != 1) {
                // Like Rampage, only status 1 is considered running. This does
                // not prove that the user saw the desired visual animation.
                api.message("Task no longer running; restoring state"); finish(false);
            } else if (now - since >= 8000) {
                api.message("ERROR: task timeout; clearing owned sequence"); finish(true);
            }
        }
    }
};
