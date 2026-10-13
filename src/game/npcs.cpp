#include "game/npcs.h"

#include "game/conditions.h"
#include "game/events.h"
#include "game/world.h"

namespace {

// NPCConditionals.CheckConditions: all conditions hold (or, with anyOf, at least one).
bool holds(const World& world, const NpcCondDef& c) {
    int passed = 0;
    for (const ConditionDef& d : c.conditions) passed += checkCondition(d, world.state, world.grid) ? 1 : 0;
    return c.anyOf ? passed > 0 : passed == (int)c.conditions.size();
}

Interactable offered(int npc) { return {Interactable::Kind::Npc, npc}; }

} // namespace

void Npcs::reset(const Area& area) {
    s_.assign(area.npcs.size(), State());
    for (size_t i = 0; i < s_.size(); i++) {
        const NpcDef& def = area.npcs[i];
        s_[i].chainIndex.assign(def.conds.size(), 0);
        s_[i].exhausted.assign(def.conds.size(), 0);
        for (const NpcCondDef& c : def.conds) s_[i].eventsRun.emplace_back(c.dialogue.size(), (uint8_t)0);
    }
}

int Npcs::lineIndex(int npc) const {
    const State& st = s_[npc];
    return st.chainIndex.empty() ? 0 : st.chainIndex[st.cond];
}

const DialogueDef* Npcs::line(const Area& area, int npc) const {
    const State& st = s_[npc];
    const NpcDef& def = area.npcs[npc];
    if (def.conds.empty()) return nullptr;
    const std::vector<DialogueDef>& chain = def.conds[st.cond].dialogue;
    const int i = st.chainIndex[st.cond];
    return i >= 0 && i < (int)chain.size() ? &chain[i] : nullptr;
}

bool Npcs::chainEmpty(const Area& area, int npc) const {
    const NpcDef& def = area.npcs[npc];
    return def.conds.empty() || def.conds[s_[npc].cond].dialogue.empty();
}

bool Npcs::dontInterruptShown(const Area& area, int npc) const {
    const DialogueDef* l = line(area, npc);
    return !chainEmpty(area, npc) && s_[npc].boxActive && l && l->dontInterrupt;
}

bool Npcs::shouldShowPing(const Area& area, int npc) const {
    return !chainEmpty(area, npc) && !s_[npc].exhausted[s_[npc].cond];
}

bool Npcs::showsPrompt(const Area& area, int npc) const {
    const DialogueDef* l = line(area, npc);
    return l && l->waitForAction && !s_[npc].typing;
}

void Npcs::start(World& world, int npc) {
    State& st = s_[npc];
    st.ping = shouldShowPing(world.area, npc); // NPCDialogueContext.Start
    // NPC.Start: pick a conditional; the first one is entered if nothing else is.
    if (!dontInterruptShown(world.area, npc)) poll(world, npc);
    if (st.cond == 0 && !world.area.npcs[npc].conds.empty()) changeCond(world, npc, 0);
    st.started = true;
}

void Npcs::poll(World& world, int npc) {
    const NpcDef& def = world.area.npcs[npc];
    // GetCondIndexWithMaxPriority: priority = list position + 1, negated when the conditions fail.
    int best = -1;
    for (int i = 0; i < (int)def.conds.size(); i++)
        if (holds(world, def.conds[i])) best = i;
    if (best != -1 && best != s_[npc].cond) changeCond(world, npc, best);
}

void Npcs::changeCond(World& world, int npc, int cond) {
    const NpcCondDef& c = world.area.npcs[npc].conds[cond];
    s_[npc].cond = cond;
    runCalls(world, c.onEnter);
    // NPCDialogueContext.OnConditionalsChanged: the new chain starts if the player is listening.
    State& st = s_[npc];
    st.ping = shouldShowPing(world.area, npc);
    if (st.inTrigger) {
        if (c.alwaysStartFromBeginning) st.chainIndex[cond] = 0;
        typeCurrent(world, npc);
        world.offer(offered(npc));
    }
}

void Npcs::typeCurrent(World& world, int npc) {
    if (chainEmpty(world.area, npc)) return;
    State& st = s_[npc];
    const DialogueDef& l = *line(world.area, npc);
    // NPCConditionals.GetDialogueString: a line waiting for the player ends in a looping " ...".
    st.text = parseDialogue(l.waitForAction ? l.text + "<type> ...</type>" : l.text, world.state);
    st.typer.start(st.text);
    st.ping = false; // DialogueDisplay.DisplaySentence
    st.typing = true;
    st.boxActive = true;
    // OnDialogueStart (doNotRepeatEvents: only the first time).
    uint8_t& ran = st.eventsRun[st.cond][st.chainIndex[st.cond]];
    if (l.noRepeatEvents) {
        if (ran & 1) return;
        ran |= 1;
    }
    runCalls(world, l.onStart);
}

void Npcs::typeNext(World& world, int npc) {
    State& st = s_[npc];
    const int count = (int)world.area.npcs[npc].conds[st.cond].dialogue.size();
    int& i = st.chainIndex[st.cond];
    if (i < count - 1) {
        i++;
        typeCurrent(world, npc);
    } else if (i == count - 1 && line(world.area, npc)->dontInterrupt) {
        deactivate(npc); // a dontInterrupt chain's end doesn't wait for the player to leave
    }
}

void Npcs::finishLine(World& world, int npc) {
    State& st = s_[npc];
    st.typing = false;
    const int cond = st.cond, index = st.chainIndex[cond];
    const std::vector<DialogueDef>& chain = world.area.npcs[npc].conds[cond].dialogue;
    if (!chain.empty()) {
        const DialogueDef& l = chain[index];
        if (l.waitForAction) {
            st.waitingForAction = true;
        } else if (!l.manual && l.delay != 0) {
            st.delayPending = true;
            st.delayFresh = true;
            st.delayLeft = l.delay;
        }
        // OnDialogueEnd (doNotRepeatEvents: only the first time), then the chain may be exhausted.
        uint8_t& ran = st.eventsRun[cond][index];
        if (!l.noRepeatEvents || !(ran & 2)) {
            ran |= 2;
            runCalls(world, l.onEnd);
            if (index == (int)chain.size() - 1 && !st.exhausted[cond]) {
                st.exhausted[cond] = 1;
                runCalls(world, world.area.npcs[npc].conds[cond].onExhausted);
            }
        }
    }
    if (index == (int)chain.size() - 1) world.withdraw(offered(npc));
}

void Npcs::deactivate(int npc) {
    State& st = s_[npc];
    st.waitingForAction = false;
    st.delayPending = false;
    st.boxActive = false;
    st.typing = false;
}

void Npcs::tryDeactivate(int npc, const Area& area) {
    const State& st = s_[npc];
    if (st.boxActive && !st.inTrigger && !dontInterruptShown(area, npc)) deactivate(npc);
}

void Npcs::triggerEnter(World& world, int npc) {
    State& st = s_[npc];
    if (!st.started) return; // (Unity retries a frame later; Starts run before triggers here)
    st.inTrigger = true;
    if (!chainEmpty(world.area, npc) && !dontInterruptShown(world.area, npc)) {
        if (world.area.npcs[npc].conds[st.cond].alwaysStartFromBeginning) st.chainIndex[st.cond] = 0;
        typeCurrent(world, npc);
    }
    world.offer(offered(npc));
}

void Npcs::triggerExit(World& world, int npc) {
    State& st = s_[npc];
    st.ping = shouldShowPing(world.area, npc);
    st.inTrigger = false;
    tryDeactivate(npc, world.area);
    world.withdraw(offered(npc));
}

void Npcs::interact(World& world, int npc) {
    State& st = s_[npc];
    if (!st.boxActive) return;
    if (st.waitingForAction) {
        st.waitingForAction = false;
        typeNext(world, npc);
    } else if (st.typing) {
        st.typer.skip();
    }
}

void Npcs::teleport(World& world, int npc, const TransformArgDef& arg) {
    const NpcDef& def = world.area.npcs[npc];
    // Reparented onto the target: the target's owner (and offset), at its position. (Placement
    // offsets add up the hierarchy, so the NPC's own is relative to its parent's.)
    const int owner = world.objects.owner(arg.object, arg.owner);
    const int parent = world.area.objects[def.object].parent;
    const float px = parent >= 0 ? world.objects.offsetX(parent) : 0, py = parent >= 0 ? world.objects.offsetY(parent) : 0;
    const float dx = arg.x + world.objects.offsetX(arg.object) - def.x - px;
    const float dy = arg.y + world.objects.offsetY(arg.object) - def.y - py;
    world.objects.setPlacement(world.area, def.object, owner, dx, dy);
}

void Npcs::update(World& world, float dt) {
    const Area& area = world.area;
    for (int i = 0; i < (int)s_.size(); i++) {
        if (!s_[i].started || !world.objects.activeInHierarchy(area.npcs[i].object)) continue;
        // NPCDialogueContext.Update, then NPC.Update.
        if (s_[i].typing && s_[i].typer.finished()) finishLine(world, i);
        tryDeactivate(i, area);
        if (!dontInterruptShown(area, i)) poll(world, i);
    }
    for (int i = 0; i < (int)s_.size(); i++) {
        if (!s_[i].started || !world.objects.activeInHierarchy(area.npcs[i].object)) continue;
        State& st = s_[i];
        if (st.delayPending) {
            if (st.delayFresh) {
                st.delayFresh = false;
            } else if ((st.delayLeft -= dt) <= 1e-4f) {
                st.delayPending = false;
                typeNext(world, i);
            }
        }
        st.typer.tick(dt); // after the delay, so a line it starts waits from the next frame
    }
}
