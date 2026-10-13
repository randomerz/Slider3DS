// NPCs (NPC.cs + NPCDialogueContext + DialogueDisplay). Each NPC's current conditional is the
// highest one in its list whose conditions hold, re-picked every frame (entering one runs its
// onConditionalEnter). While the player is in the NPC's dialogue trigger it types the current
// conditional's dialogue chain above its head, line by line: a line moves on after its delay, on
// the action button (waitForAction: a looping " ..." shows), or by an event (manual); the action
// button also skips typing. Leaving the trigger clears the text unless the line is dontInterrupt.
// Lines' onStart / onEnd and the chain's onDialogueChainExhausted (after its last line) run
// UnityEvent calls. The message ping shows while the current chain hasn't been read to its end.
// Not yet: emotes, animations, walks, cutscene state.
#pragma once

#include <cstdint>
#include <vector>

#include "game/area.h"
#include "game/dialogue_text.h"

class World;

class Npcs {
public:
    void reset(const Area& area); // no NPC started (a fresh scene load)
    void start(World& world, int npc); // NPC.Start, on its first activation (Starts)
    // Every started NPC whose object is active: dialogue finishes or clears, the conditional is
    // re-picked; then the typing / delay clocks advance (Unity's coroutines, after Update).
    void update(World& world, float dt);

    void triggerEnter(World& world, int npc); // NPC.OnDialogueTriggerEnter
    void triggerExit(World& world, int npc);  // NPC.OnDialogueTriggerExit
    void interact(World& world, int npc);     // the action button while it's on offer
    void typeCurrentLine(World& world, int npc) { typeCurrent(world, npc); } // NPC.TypeCurrentDialogue
    bool showsPrompt(const Area& area, int npc) const; // DisplayInteractionPrompt
    // NPC.Teleport: moves the NPC onto `arg`'s position, following it from then on.
    static void teleport(World& world, int npc, const TransformArgDef& arg);

    // For drawing and tests.
    int currentCond(int npc) const { return s_[npc].cond; }
    int lineIndex(int npc) const; // in the current conditional's chain
    bool textShown(int npc) const { return s_[npc].boxActive; }
    const DialogueText& text(int npc) const { return s_[npc].text; }
    const Typer& typer(int npc) const { return s_[npc].typer; }
    bool pingShown(int npc) const { return s_[npc].ping; }

private:
    struct State {
        bool started = false;
        int cond = 0;                    // currCondIndex
        std::vector<int> chainIndex;     // per conditional: the line it's at (cachedDchainIndices)
        std::vector<uint8_t> exhausted;  // per conditional: its last line was shown
        std::vector<std::vector<uint8_t>> eventsRun; // per conditional, line: bit0 onStart, bit1 onEnd ran
        bool inTrigger = false, boxActive = false, typing = false, waitingForAction = false, ping = false;
        bool delayPending = false, delayFresh = false; // SetNextDialogueInChainAfterDelay
        float delayLeft = 0;
        DialogueText text;
        Typer typer;
    };

    const DialogueDef* line(const Area& area, int npc) const; // CurrentDialogue(), null if none
    bool chainEmpty(const Area& area, int npc) const;
    bool dontInterruptShown(const Area& area, int npc) const; // NPCGivingDontInterruptDialogue
    bool shouldShowPing(const Area& area, int npc) const;
    void poll(World& world, int npc);              // PollForNewConditional
    void changeCond(World& world, int npc, int cond);
    void typeCurrent(World& world, int npc);       // TypeCurrentDialogue
    void typeNext(World& world, int npc);          // TypeNextDialogueInChain
    void finishLine(World& world, int npc);        // HandleDialogueFinished
    void tryDeactivate(int npc, const Area& area);
    void deactivate(int npc);

    std::vector<State> s_;
};
