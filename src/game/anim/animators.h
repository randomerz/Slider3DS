// Runtime state of the Area's Animators (Area::animators: the NPCs', the player's, the abducted cat's
// and its bubble's, the trees' and bushes'): Unity's Animator for sprite clips, as far as the exported
// controllers use it. Each has a current state (playing its clip at the state's speed), bool, trigger,
// int and float parameters and the controller's transitions: once a frame the first transition
// whose conditions hold (and exit time is reached: from then on for 1 or more, as it's passed in a
// loop below 1) switches state, instantly, resetting the triggers it used.
// Play(state) jumps to a state; switching the controller starts over in the new one's default.
// An Animator only runs while its object is active; re-enabling one rebinds it (default state and
// parameters) unless keepAnimatorStateOnDisable. Clips' transform curves move their targets
// (AnimatorDef::targets: an item holder bobbing; scene objects via their ObjectStates placement);
// with writeDefaults, a state that doesn't move a target puts it back where it was authored. An
// Animator owns its targets' placement: nothing else may set it (asserted), and reset() (with
// ObjectStates::init, World::reloadObjects) puts both back as authored.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "game/data/area.h"

class World;

class Animators {
public:
    void reset(const Area& area); // every Animator in its default state, not yet enabled

    // OnEnable / OnDisable: Animators whose object became active since the last call rebind (unless
    // keepState). Call it right after objects may have been activated, before their Starts and scripts.
    void enable(const World& world);
    // One frame: advances the enabled Animators' time, takes transitions, records the sprite each
    // now shows and moves their targets.
    void update(World& world, float dt);

    // These first catch up with the Animator's object activity (Unity's OnEnable runs inside
    // SetActive, before anything else can play on it).
    // Animator.Play(stateName): to that state from its start (no-op if unknown, or already in it
    // unless `restart`).
    void play(const World& world, int animator, const std::string& state, bool restart = false);
    // Animator.SetBool (no-op if the controller has no such parameter).
    void setBool(const World& world, int animator, const std::string& param, bool value);
    // Animator.SetTrigger: set until a transition uses it (no-op if the controller has no such parameter).
    void setTrigger(const World& world, int animator, const std::string& param) { setBool(world, animator, param, true); }
    // Animator.SetInteger / SetFloat (no-op if the controller has no such parameter).
    void setFloat(const World& world, int animator, const std::string& param, float value);
    // Any of those by the controller's parameter index (resolved at load; a trigger or bool: 0 / 1).
    void setParam(const World& world, int animator, int param, float value);
    // Animator.runtimeAnimatorController = Area::animatorControllers[controller].
    void setController(const World& world, int animator, int controller);

    // The sprite the Animator's object shows: an Area::sprites index, NULL_SPRITE (a null key: nothing),
    // or NOT_YET (no frame yet: use the exported one).
    static constexpr int NOT_YET = -1;
    int sprite(int animator) const { return s_[animator].sprite; }
    int controller(int animator) const { return s_[animator].controller; } // Area::animatorControllers index
    // Where AnimatorDef::targets[target] is now (its localPosition).
    void targetPosition(int animator, int target, float& x, float& y) const {
        x = s_[animator].targetX[target];
        y = s_[animator].targetY[target];
    }
    const std::string& stateName(const Area& area, int animator) const;
    // The clip events (AnimationEvents) the last update passed, in order: the Animator and the
    // event's function (e.g. PlayerRun's "Step").
    struct Fired {
        int animator;
        const std::string* function;
    };
    const std::vector<Fired>& fired() const { return fired_; }
    bool param(const Area& area, int animator, const std::string& name) const { return paramValue(area, animator, name) != 0; }
    float paramValue(const Area& area, int animator, const std::string& name) const; // 0 if none

private:
    struct State {
        int controller = 0;
        int state = 0;
        float time = 0;           // seconds into the state (times its speed)
        std::vector<float> params; // (a bool's or trigger's 0 / 1)
        int sprite = NOT_YET;     // last frame shown (a state without a clip keeps it)
        bool enabled = false;     // its object was active at the last update
        std::vector<float> targetX, targetY; // per AnimatorDef::targets entry: its localPosition now
    };
    void applyMoves(World& world, int animator); // the current state's transform curves
    void rebind(const Area& area, State& st); // default state, default parameters
    void syncEnabled(const World& world, int animator); // enable() for one Animator
    void refreshSprite(const Area& area, State& st);

    std::vector<State> s_;
    std::vector<Fired> fired_;
    std::vector<float> scratchX_, scratchY_; // applyMoves
};
