#include "../src/twirl_session.hpp"
#include <cassert>
#include <string>
#include <vector>
#include <cstdio>
struct FakeGame {
    bool valid = true, resources = true, hand = true, acceptSet = true;
    int seq = 12, taskStatus = 1;
    std::vector<std::string> events;
    bool validActor(int) { return valid; }
    void requestResources() { events.push_back("request"); }
    bool loaded() { return resources; }
    bool drawn(int, int) { return hand; }
    void equip(int, int) { events.push_back("equip"); }
    void ready(int, bool on) { events.push_back(on ? "ready on" : "ready off"); }
    bool applySet(int) { events.push_back("set"); return acceptSet; }
    int openSequence() { events.push_back("open"); return seq; }
    void addAction() { events.push_back("action"); }
    void closeSequence() { events.push_back("close"); }
    void perform(int, int) { events.push_back("perform"); }
    void release(int) { events.push_back("release"); }
    int status(int) { return taskStatus; }
    void clear(int) { events.push_back("clear"); }
    void resetTree(int) { events.push_back("reset tree"); }
    void resetSet(int) { events.push_back("reset set"); }
    void message(const char*) {}
};
int main() {
    { // completion: ordered sequence + one release + paired readymode restoration
        FakeGame g; TwirlSession s(g); s.begin(1, 7, 0, 0);
        s.tick(1, 1, false); s.tick(1, 2, false); s.tick(1, 3, false);
        assert(s.active()); g.taskStatus = 0; s.tick(1, 4, false);
        assert(!s.active());
        const std::vector<std::string> want{"request","ready on","set","open","action","close","perform","release","reset set","ready off"};
        assert(g.events == want);
        s.finish(true); assert(g.events == want); // cleanup is idempotent
    }
    { // missing tree/set must never change actor or player state
        FakeGame g; g.resources = false; TwirlSession s(g); s.begin(1,7,0,0);
        s.tick(1,3000,false); assert(!s.active());
        assert(g.events == std::vector<std::string>{"request"});
    }
    { // invalid/partial set application still restores owned state
        FakeGame g; g.acceptSet = false; TwirlSession s(g); s.begin(1,7,0,0);
        s.tick(1,1,false); s.tick(1,2,false);
        assert(!s.active());
        assert((g.events == std::vector<std::string>{"request","ready on","set","reset set","ready off"}));
    }
    { // sequence allocation failure must not leak ready mode
        FakeGame g; g.seq = -1; TwirlSession s(g); s.begin(1,7,0,0);
        s.tick(1,1,false); s.tick(1,2,false);
        assert(!s.active());
        assert((g.events == std::vector<std::string>{"request","ready on","set","open","reset set","ready off"}));
    }
    for (int cancelType = 0; cancelType < 4; ++cancelType) {
        FakeGame g; TwirlSession s(g); s.begin(1,7,0,0);
        s.tick(1,1,false); s.tick(1,2,false);
        if (cancelType == 0) s.tick(1,3,true); // F10/pause/death cancellation
        if (cancelType == 1) s.tick(2,3,false); // replacement actor
        if (cancelType == 2) s.tick(1,8002,false); // stuck action
        if (cancelType == 3) { g.valid=false; s.tick(1,3,false); }
        assert(!s.active()); assert(g.events.back() == "ready off");
        if (g.valid) {
            auto n=g.events.size();
            assert(g.events[n-4]=="clear" && g.events[n-3]=="reset tree" && g.events[n-2]=="reset set");
        }
    }
    { // first tap already holstered weapon: wait for redrawing before set/task
        FakeGame g; g.hand=false; TwirlSession s(g); s.begin(1,7,0,0);
        s.tick(1,1,false); s.tick(1,2,false);
        assert((g.events == std::vector<std::string>{"request","equip"}));
        g.hand=true; s.tick(1,3,false); assert(s.active());
        s.tick(1,4,true); assert(g.events.back()=="ready off");
    }
    { // draw failure must not change animation or readymode
        FakeGame g; g.hand=false; TwirlSession s(g); s.begin(1,7,0,0);
        s.tick(1,1,false); s.tick(1,1001,false);
        assert(!s.active()); assert((g.events==std::vector<std::string>{"request","equip"}));
    }
    { // resource timeout across tick rollover
        FakeGame g; g.resources=false; TwirlSession s(g); s.begin(1,7,0,UINT32_MAX-100);
        s.tick(1,3000,false); assert(!s.active());
    }
    std::puts("PASS: action order, completion, idempotent cleanup, load/draw/set/sequence failures, cancellation, actor loss, watchdog, tick rollover");
}
