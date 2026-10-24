#include "game/npc/npcs.h"

#include <cmath>

#include "game/events/conditions.h"
#include "game/events/events.h"
#include "game/world/world.h"

namespace {

// NPCConditionals.CheckConditions: all conditions hold (or, with anyOf, at least one).
bool holds(const World& world, const NpcCondDef& c) {
    int passed = 0;
    for (const ConditionDef& d : c.conditions) passed += checkCondition(d, world) ? 1 : 0;
    return c.anyOf ? passed > 0 : passed == (int)c.conditions.size();
}

Interactable offered(int npc) { return {Interactable::Kind::Npc, npc}; }

} // namespace

void Npcs::reset(const Area& area) {
    s_.assign(area.npcs.size(), State());
    random_ = VoiceRandom();
    dialogueEnabled_ = true;
    for (size_t i = 0; i < s_.size(); i++) {
        const NpcDef& def = area.npcs[i];
        for (const NpcCondDef& c : def.conds)
            for (const ConditionDef& d : c.conditions)
                if (!conditionReadsOnlyStateAndGrid(d)) s_[i].pollsEveryFrame = true;
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

bool Npcs::canUpdateConds(const Area& area, int npc) const {
    return dialogueEnabled_ && !dontInterruptShown(area, npc);
}

bool Npcs::shouldShowPing(const Area& area, int npc) const {
    return !chainEmpty(area, npc) && !s_[npc].exhausted[s_[npc].cond];
}

bool Npcs::pingShown(const World& world, int npc) const {
    const int ping = world.area.npcs[npc].pingObject;
    return ping >= 0 && world.objects.activeInHierarchy(ping);
}

void Npcs::setPing(World& world, int npc, bool shown) {
    const int ping = world.area.npcs[npc].pingObject;
    if (ping >= 0) world.objects.setActive(world.area, ping, shown);
}

void Npcs::setEmote(World& world, int npc, int emote) {
    const NpcEmoteDef& e = world.area.npcs[npc].emote;
    State& st = s_[npc];
    if (e.object < 0) return;
    // UpdateEmotePosition (even when the emote doesn't change): beside the face, by the NPC sprite's flip.
    const int sprite = world.area.npcs[npc].sprite;
    st.emoteLeft = e.facesRight == (sprite >= 0 && world.objects.flipX(sprite));
    if (st.emote == emote) return;
    st.emote = emote;
    // Animator.Play(name); shown again after None it starts over.
    if (emote != 0) world.animators.play(world, e.animator, NpcEmoteDef::name(emote), true);
}

void Npcs::playAnimation(World& world, int npc, const std::string& state) {
    const int animator = world.area.npcs[npc].animator;
    if (animator < 0 || !world.objects.activeInHierarchy(world.area.animators[animator].object)) return;
    std::string& current = s_[npc].animation;
    if (state.empty()) {
        // "" = back to the default animation (if a named one played).
        if (current.empty()) return;
        current.clear();
        world.animators.play(world, animator, "Idle");
        return;
    }
    if (state == current) return;
    current = state;
    world.animators.play(world, animator, state);
}

bool Npcs::emoteSprite(const World& world, int npc, SpritePlacement& out) const {
    const State& st = s_[npc];
    const NpcEmoteDef& e = world.area.npcs[npc].emote;
    // (No frame yet, or a null key: nothing to show either way.)
    if (st.emote == 0 || e.animator < 0 || world.animators.sprite(e.animator) < 0) return false;
    out = e.place;
    out.anim = -1;
    out.sprite = world.animators.sprite(e.animator); // its Animator's frame (intro, then the loop state)
    const float side = e.localX < 0 ? -e.localX : e.localX;
    out.x = e.place.x - e.localX + (st.emoteLeft ? -side : side);
    return true;
}

bool Npcs::showsPrompt(const Area& area, int npc) const {
    const DialogueDef* l = line(area, npc);
    return l && l->waitForAction && !s_[npc].typing;
}

void Npcs::start(World& world, int npc) {
    State& st = s_[npc];
    setPing(world, npc, shouldShowPing(world.area, npc)); // NPCDialogueContext.Start
    // NPC.Start: pick a conditional; the first one is entered if nothing else is.
    if (canUpdateConds(world.area, npc)) poll(world, npc);
    if (st.cond == 0 && !world.area.npcs[npc].conds.empty()) changeCond(world, npc, 0);
    st.started = true;
    st.wasActive = true;
}

void Npcs::teleport(World& world, int npc, int target) {
    const int object = world.area.npcs[npc].object;
    float ax, ay, bx, by;
    world.objectPosition(object, ax, ay);
    world.objectPosition(target, bx, by);
    if (std::fabs(ax - bx) > 1e-4f || std::fabs(ay - by) > 1e-4f) world.objects.moveOnto(world.area, object, target);
}

void Npcs::poll(World& world, int npc) {
    const NpcDef& def = world.area.npcs[npc];
    // GetCondIndexWithMaxPriority: priority = list position + 1, negated when the conditions fail.
    int best = -1;
    for (int i = 0; i < (int)def.conds.size(); i++)
        if (holds(world, def.conds[i])) best = i;
    State& st = s_[npc];
    st.polled = true;
    st.stateVersion = world.state.version();
    st.gridVersion = world.grid.version();
    if (best != -1 && best != st.cond) changeCond(world, npc, best);
}

void Npcs::changeCond(World& world, int npc, int cond) {
    const NpcCondDef& c = world.area.npcs[npc].conds[cond];
    s_[npc].cond = cond;
    runCalls(world, c.onEnter);
    playAnimation(world, npc, c.animationOnEnter);
    setEmote(world, npc, c.emote);
    // NPCDialogueContext.OnConditionalsChanged: the new chain starts if the player is listening.
    State& st = s_[npc];
    setPing(world, npc, shouldShowPing(world.area, npc));
    if (st.inTrigger) {
        if (c.alwaysStartFromBeginning) st.chainIndex[cond] = 0;
        typeCurrent(world, npc);
        world.offer(offered(npc));
    }
}

void Npcs::typeCurrent(World& world, int npc) {
    if (!dialogueEnabled_ || chainEmpty(world.area, npc)) return;
    State& st = s_[npc];
    const DialogueDef& l = *line(world.area, npc);
    // NPCConditionals.GetDialogueString: a line waiting for the player ends in a looping " ...".
    st.text = parseDialogue(l.waitForAction ? l.text + "<type> ...</type>" : l.text, world.state);
    // DialogueDisplay.DisplaySentence: with a voice, the line types at its pace (its length over the
    // text's), the music dips while it talks, and it starts reading — unless still reading the last.
    float speed = Typer::TEXT_SPEED;
    const VoiceDef& voice = world.area.npcs[npc].voice;
    if (voice.present && !st.text.chars.empty()) {
        VoiceLine read = vocalize(voice, l.emoteOnStart, st.text.chars, 1, random_);
        speed = read.totalDuration / st.text.chars.size();
        world.audio.dampenMusic(0.6f, read.totalDuration + 0.2f, npc + 1);
        if (!st.speaking) {
            st.voice = std::move(read);
            st.voiceStep = 0;
            st.voiceWait = 0;
            st.speaking = true;
            speak(world, npc); // (its first syllable at once: StartCoroutine runs to its first wait)
        } else {
            // Still reading: the paragraph's sentences are replaced under the running coroutine —
            // it finishes the clause it's in, then goes on with the new line's next clauses.
            const int clause = st.voiceStep > 0 ? st.voice.steps[st.voiceStep - 1].clause : -1;
            std::vector<VoiceStep> next;
            for (size_t k = st.voiceStep; k < st.voice.steps.size() && st.voice.steps[k].clause == clause; k++)
                next.push_back(st.voice.steps[k]);
            for (const VoiceStep& s : read.steps)
                if (s.clause > clause) next.push_back(s);
            st.voice.steps = std::move(next);
            st.voice.totalDuration = read.totalDuration;
            st.voiceStep = 0;
        }
    }
    st.typer.start(st.text, speed);
    st.textVersion = ++textCounter_;
    setPing(world, npc, false); // DialogueDisplay.DisplaySentence
    st.typing = true;
    st.boxActive = true;
    // OnDialogueStart: the line's animation and emote, then its onStart (doNotRepeatEvents: only the
    // first time).
    playAnimation(world, npc, l.animationOnStart);
    setEmote(world, npc, l.emoteOnStart);
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
        deactivate(world, npc); // a dontInterrupt chain's end doesn't wait for the player to leave
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
            st.delay.start(l.delay, world.frame);
            st.delayDue = false;
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

void Npcs::speak(World& world, int npc) {
    State& st = s_[npc];
    const NpcDef& n = world.area.npcs[npc];
    while (st.speaking && st.voiceWait <= 0) {
        if (st.voiceStep >= st.voice.steps.size()) {
            st.speaking = false;
            break;
        }
        const VoiceStep& s = st.voice.steps[st.voiceStep++];
        if (s.sound) {
            float x, y;
            world.objectPosition(n.object, x, y);
            world.audio.playVoice(n.voice.event, npc + 1, s.vowel, s.duration, s.pitchStart, s.pitchMid, s.pitchEnd,
                                  st.voice.volume, x, y);
        }
        st.voiceWait += s.duration;
    }
}

void Npcs::stopVoice(World& world, int npc) {
    if (!world.area.npcs[npc].voice.present) return;
    s_[npc].speaking = false;
    world.audio.stopVoice(npc + 1);
}

void Npcs::deactivate(World& world, int npc) {
    if (const DialogueDef* l = line(world.area, npc)) {
        playAnimation(world, npc, l->animationOnLeave);
        setEmote(world, npc, l->emoteOnLeave);
    }
    State& st = s_[npc];
    st.waitingForAction = false;
    st.delay.stop();
    st.delayDue = false;
    st.boxActive = false;
    st.typing = false;
    stopVoice(world, npc); // (FadeAwayDialogue)
}

void Npcs::tryDeactivate(World& world, int npc) {
    const State& st = s_[npc];
    if (st.boxActive && !st.inTrigger && !dontInterruptShown(world.area, npc)) deactivate(world, npc);
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
    setPing(world, npc, shouldShowPing(world.area, npc));
    st.inTrigger = false;
    tryDeactivate(world, npc);
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

void Npcs::update(World& world, float dt) {
    const Area& area = world.area;
    for (int i = 0; i < (int)s_.size(); i++) {
        State& st = s_[i];
        if (!st.started) continue;
        const bool active = world.objects.activeInHierarchy(area.npcs[i].object);
        if (!active && st.wasActive) {
            // Deactivation stops its coroutines: a pending delay is dropped, typing freezes (A can
            // still skip it).
            st.delay.stop();
            st.delayDue = false;
            st.typer.halt();
            stopVoice(world, i); // (its coroutine stops too)
        }
        st.wasActive = active;
        if (!active) continue;
        // NPCDialogueContext.Update, then NPC.Update (re-picking the conditional; skipped while the
        // game state and grid are unchanged, if that's all its conditions read).
        if (st.typing && st.typer.finished()) finishLine(world, i);
        tryDeactivate(world, i);
        const bool changed = !st.polled || st.pollsEveryFrame || st.stateVersion != world.state.version() ||
                             st.gridVersion != world.grid.version();
        if (canUpdateConds(area, i) && changed) poll(world, i);
    }
    for (int i = 0; i < (int)s_.size(); i++) {
        if (!s_[i].started || !world.objects.activeInHierarchy(area.npcs[i].object)) continue;
        State& st = s_[i];
        // (run out while dialogue is off: it waits for it — Unity re-checks every 0.25 s, so the port's
        // line can come up to 0.25 s sooner. Like Unity's, a pending delay outlives a conditional
        // change: it then advances the new chain.)
        if (st.delay.done(world.frame, dt)) st.delayDue = true;
        if (st.delayDue && dialogueEnabled_) {
            st.delayDue = false;
            typeNext(world, i);
        }
        st.typer.tick(dt); // after the delay, so a line it starts waits from the next frame
        if (st.speaking) {
            st.voiceWait -= dt;
            speak(world, i);
        }
    }
}
