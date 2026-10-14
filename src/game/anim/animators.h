// Runtime state of the Area's Animators (Area::animators: the NPCs' and the player's): Unity's Animator for
// sprite clips, as far as the exported controllers use it. Each has a current state (playing its
// clip at the state's speed), bool parameters and the controller's transitions: once a frame the
// first transition whose conditions hold (and exit time has passed) switches state, instantly.
// Play(state) jumps to a state; switching the controller starts over in the new one's default.
// An Animator only runs while its object is active; re-enabling one rebinds it (default state and
// parameters) unless keepAnimatorStateOnDisable.
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
    // One frame: advances the enabled Animators' time, takes transitions, and records the sprite
    // each now shows.
    void update(const World& world, float dt);

    // Animator.Play(stateName): to that state from its start (no-op if unknown or already in it).
    void play(const Area& area, int animator, const std::string& state);
    // Animator.SetBool (no-op if the controller has no such parameter).
    void setBool(const Area& area, int animator, const std::string& param, bool value);
    // Animator.runtimeAnimatorController = Area::animatorControllers[controller].
    void setController(const Area& area, int animator, int controller);

    // The sprite (Area::sprites index) the Animator's object shows, -1 = none yet (use the exported one).
    int sprite(int animator) const { return s_[animator].sprite; }
    const std::string& stateName(const Area& area, int animator) const;
    bool param(const Area& area, int animator, const std::string& name) const;

private:
    struct State {
        int controller = 0;
        int state = 0;
        float time = 0;           // seconds into the state (times its speed)
        std::vector<uint8_t> params;
        int sprite = -1;          // last frame shown (a state without a clip keeps it)
        bool enabled = false;     // its object was active at the last update
    };
    void rebind(const Area& area, State& st); // default state, default parameters
    void refreshSprite(const Area& area, State& st);

    std::vector<State> s_;
};
