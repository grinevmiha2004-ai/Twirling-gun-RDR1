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
template<class Session> void prepare(Session& s) {
    s.begin(1,7,0,0);
    s.tick(1,1,false);   // loaded, wait for release
    s.tick(1,2,false);   // released
    s.tick(1,152,false);// set + equip
    s.tick(1,153,false);// first drawn sample
    s.tick(1,253,false);// stable 100ms: submit
}
int main() {
    {
        FakeGame g; TwirlSession s(g); prepare(s);
        assert(s.active()); g.taskStatus=0; s.tick(1,254,false);
        const std::vector<std::string> want{"request","ready on","set","equip","open","action","close","perform","release","reset set","ready off"};
        assert(!s.active()); assert(g.events==want); s.finish(true); assert(g.events==want);
    }
    {
        FakeGame g; g.resources=false; TwirlSession s(g); s.begin(1,7,0,0);
        s.tick(1,3000,false); assert(!s.active()); assert(g.events==std::vector<std::string>{"request"});
    }
    {
        FakeGame g; g.acceptSet=false; TwirlSession s(g); prepare(s);
        assert(!s.active());
        assert((g.events==std::vector<std::string>{"request","ready on","set","reset set","ready off"}));
    }
    {
        FakeGame g; g.seq=-1; TwirlSession s(g); prepare(s);
        assert(!s.active());
        assert((g.events==std::vector<std::string>{"request","ready on","set","equip","open","reset set","ready off"}));
    }
    for (int kind=0;kind<4;++kind) {
        FakeGame g; TwirlSession s(g); prepare(s);
        if (kind==0) s.tick(1,254,true);
        if (kind==1) s.tick(2,254,false);
        if (kind==2) s.tick(1,8253,false);
        if (kind==3) { g.valid=false; s.tick(1,254,false); }
        assert(!s.active()); assert(g.events.back()=="ready off");
        if(g.valid) { auto n=g.events.size(); assert(g.events[n-4]=="clear" && g.events[n-3]=="reset tree" && g.events[n-2]=="reset set"); }
    }
    { // second button press remains held: no actor mutation or sequence
        FakeGame g; TwirlSession s(g); s.begin(1,7,0,0);
        s.tick(1,1,false,true); s.tick(1,500,false,true);
        assert(g.events==std::vector<std::string>{"request"});
        s.tick(1,501,false,false); s.tick(1,650,false,false);
        assert(g.events==std::vector<std::string>{"request"});
        s.tick(1,651,false,false);
        assert((g.events==std::vector<std::string>{"request","ready on","set","equip"}));
        s.finish(false);
    }
    { // a transient drawn sample followed by holstering must reset stability
        FakeGame g; TwirlSession s(g); s.begin(1,7,0,0);
        s.tick(1,1,false); s.tick(1,2,false); s.tick(1,152,false);
        s.tick(1,153,false); g.hand=false; s.tick(1,253,false);
        assert(g.events.back()=="equip");
        g.hand=true; s.tick(1,300,false); s.tick(1,399,false);
        assert(g.events.back()=="equip");
        s.tick(1,400,false); assert(g.events.back()=="release"); s.finish(true);
    }
    { // permanently missing weapon: fail instead of spinning an empty hand
        FakeGame g; g.hand=false; TwirlSession s(g); s.begin(1,7,0,0);
        s.tick(1,1,false); s.tick(1,2,false); s.tick(1,152,false); s.tick(1,1652,false);
        assert(!s.active()); assert(g.events.back()=="ready off");
        for(const auto& e:g.events) assert(e!="perform");
    }
    { // no equip/holster calls during playback: natural spin ending owns weapon
        FakeGame g; TwirlSession s(g); prepare(s);
        auto count=g.events.size(); s.tick(1,300,false); s.tick(1,600,false);
        assert(g.events.size()==count); s.finish(true);
    }
    { // cancellation while the button is held never changes the actor
        FakeGame g; TwirlSession s(g); s.begin(1,7,0,0);
        s.tick(1,1,false,true); s.tick(1,3001,false,true);
        assert(!s.active()); assert(g.events==std::vector<std::string>{"request"});
    }
    { // resource timeout across tick rollover
        FakeGame g; g.resources=false; TwirlSession s(g); s.begin(1,7,0,UINT32_MAX-100);
        s.tick(1,3000,false); assert(!s.active());
    }
    std::puts("PASS: release gate, post-set equip, stable weapon gate, transition regression, missing weapon, action order, no playback equip, completion, failure cleanup, cancel, watchdog, rollover");
}
