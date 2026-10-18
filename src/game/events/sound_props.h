// CowProp / DogProp (the Moo / Bark calls): a prop that sounds at most once per cooldown and shows
// its emote for a second each time (TurnOffEmoteAfterDelay; a new sound restarts the second).
#pragma once

#include <vector>

#include "game/world/wait.h"

class World;

class SoundProps {
public:
    static constexpr float EMOTE_TIME = 1; // TurnOffEmoteAfterDelay(1)

    void reset() { props_.clear(); }
    // A Moo / Bark on `object` (`emote`: its emote object, -1 = none): false while it cools down;
    // else its cooldown starts and its emote shows (again) for EMOTE_TIME.
    bool use(World& world, int object, int emote, float cooldown);
    void update(float dt);                   // their Update: the cooldowns run down
    void coroutines(World& world, float dt); // the emotes go off

private:
    struct Prop {
        int object, emote;
        float cooldown;
        Wait emoteOff;
    };
    std::vector<Prop> props_;
};
