// NPCs (NPC.cs + NPCDialogueContext + DialogueDisplay). Each NPC's current conditional is the
// highest one in its list whose conditions hold, re-checked whenever what they read may have changed (entering one runs its
// onConditionalEnter). While the player is in the NPC's dialogue trigger it types the current
// conditional's dialogue chain above its head, line by line: a line moves on after its delay, on
// the action button (waitForAction: a looping " ..." shows), or by an event (manual); the action
// button also skips typing. Leaving the trigger clears the text unless the line is dontInterrupt.
// Lines' onStart / onEnd and the chain's onDialogueChainExhausted (after its last line) run
// UnityEvent calls. The message ping (an object: SetActive) shows while the current chain hasn't
// been read to its end. Emotes (NPCEmotes) beside the head: set by a conditional's emoteOnEnter, a
// line's emoteOnStart, and its emoteOnLeave when the box closes; they sit on the side the NPC faces
// (as of when they were set). Animations (NPCAnimatorController.Play on its Animator: game/anim) are
// played by the same three moments: animationOnEnter, animationOnStart, animationOnLeave.
// Hiding an NPC stops its clocks (Unity stops a disabled object's coroutines).
// Conditionals are re-checked only when the game state or the grid changed.
// Cutscenes switch dialogue off for every NPC (NPCDialogueContext.dialogueEnabledAllNPC): no line
// starts (one typing finishes), a delayed next line waits for it, conditionals aren't re-picked.
// Walks: game/npc/npc_walks.
#pragma once

#include <cstdint>
#include <vector>

#include "game/data/area.h"
#include "game/npc/dialogue_text.h"
#include "game/npc/vocalizer.h"

class World;

class Npcs {
public:
    void reset(const Area& area); // no NPC started (a fresh scene load)
    void start(World& world, int npc); // NPC.Start, on its first activation (Starts)
    // Every started NPC whose object is active: dialogue finishes or clears, the conditional is
    // re-picked (if anything it could depend on changed); then the typing / delay clocks advance
    // (Unity's coroutines, after Update).
    void update(World& world, float dt);

    void triggerEnter(World& world, int npc); // NPC.OnDialogueTriggerEnter
    void triggerExit(World& world, int npc);  // NPC.OnDialogueTriggerExit
    void interact(World& world, int npc);     // the action button while it's on offer
    void typeCurrent(World& world, int npc);  // NPC.TypeCurrentDialogue
    // DialogueDisplay.Activate / DeactivateMessagePing (SetMessagePing): its ping object on or off.
    void setPing(World& world, int npc, bool shown);
    // NPC.Teleport: onto `target` (an object), parented to it; nothing if it's already there.
    void teleport(World& world, int npc, int target);
    bool showsPrompt(const Area& area, int npc) const; // DisplayInteractionPrompt
    // NPCDialogueContext.dialogueEnabledAllNPC (on again after a scene load).
    void setDialogueEnabled(bool on) { dialogueEnabled_ = on; }
    bool dialogueEnabled() const { return dialogueEnabled_; }

    // For drawing and tests.
    int currentCond(int npc) const { return s_[npc].cond; }
    bool pollsEveryFrame(int npc) const { return s_[npc].pollsEveryFrame; }
    int lineIndex(int npc) const; // in the current conditional's chain
    bool textShown(int npc) const { return s_[npc].boxActive; }
    const DialogueText& text(int npc) const { return s_[npc].text; }
    const Typer& typer(int npc) const { return s_[npc].typer; }
    const VoiceLine& voiceLine(int npc) const { return s_[npc].voice; } // what its voice read last (tests)
    // Identifies the line being shown: new for every line typed (also across reset()), for caches.
    uint32_t textVersion(int npc) const { return s_[npc].textVersion; }
    bool pingShown(const World& world, int npc) const;
    int emote(int npc) const { return s_[npc].emote; } // NpcEmoteDef value shown, 0 = none
    // The emote sprite as it's drawn now (placement relative to the emote object's base), if one shows.
    bool emoteSprite(const World& world, int npc, SpritePlacement& out) const;

private:
    struct State {
        bool started = false;
        int cond = 0;                    // currCondIndex
        std::vector<int> chainIndex;     // per conditional: the line it's at (cachedDchainIndices)
        std::vector<uint8_t> exhausted;  // per conditional: its last line was shown
        std::vector<std::vector<uint8_t>> eventsRun; // per conditional, line: bit0 onStart, bit1 onEnd ran
        bool inTrigger = false, boxActive = false, typing = false, waitingForAction = false;
        bool delayPending = false, delayFresh = false; // SetNextDialogueInChainAfterDelay
        float delayLeft = 0;
        bool wasActive = false;           // object active at the last update
        bool polled = false;              // conditionals checked against the versions below
        bool pollsEveryFrame = false;     // a condition reads more than GameState + Grid
        uint32_t stateVersion = 0, gridVersion = 0;
        uint32_t textVersion = 0;
        std::string animation;   // NPCAnimatorController.currentState ("" = the default ones)
        int emote = 0;           // NPCEmotes.currentEmote
        bool emoteLeft = false;  // its side, decided when it was last set
        DialogueText text;
        Typer typer;
        // Its voice reading the line (VocalizableParagraph's coroutine): the steps, the next one,
        // the time left on the current one.
        VoiceLine voice;
        size_t voiceStep = 0;
        float voiceWait = 0;
        bool speaking = false;
    };

    const DialogueDef* line(const Area& area, int npc) const; // CurrentDialogue(), null if none
    bool chainEmpty(const Area& area, int npc) const;
    bool dontInterruptShown(const Area& area, int npc) const; // NPCGivingDontInterruptDialogue
    bool canUpdateConds(const Area& area, int npc) const;     // NPC.CanUpdateConditionals
    bool shouldShowPing(const Area& area, int npc) const;
    void poll(World& world, int npc);              // PollForNewConditional
    void changeCond(World& world, int npc, int cond);
    void typeNext(World& world, int npc);          // TypeNextDialogueInChain
    void finishLine(World& world, int npc);        // HandleDialogueFinished
    void tryDeactivate(World& world, int npc);
    void deactivate(World& world, int npc);       // DeactivateDialogueBox
    void setEmote(World& world, int npc, int emote); // NPCEmotes.SetEmote
    void playAnimation(World& world, int npc, const std::string& state); // NPCAnimatorController.Play
    void speak(World& world, int npc);     // the voice's steps that are due
    void stopVoice(World& world, int npc); // VocalizableParagraph.Stop

    std::vector<State> s_;
    VoiceRandom random_; // (the vocalizer's choices: stress, intonation)
    bool dialogueEnabled_ = true;
    uint32_t textCounter_ = 0; // never reset: a cache keyed by textVersion can't see an old value again
};
