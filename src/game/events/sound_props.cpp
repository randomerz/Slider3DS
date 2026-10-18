#include "game/events/sound_props.h"

#include "game/world/world.h"

bool SoundProps::use(World& world, int object, int emote, float cooldown) {
    Prop* p = nullptr;
    for (Prop& q : props_)
        if (q.object == object) p = &q;
    if (!p) {
        props_.push_back({object, emote, 0, {}});
        p = &props_.back();
    }
    if (p->cooldown > 0) return false; // (timeUntilCanMoo <= 0)
    p->cooldown = cooldown;
    if (p->emote >= 0) {
        world.objects.setActive(world.area, p->emote, true);
        p->emoteOff.start(EMOTE_TIME, world.frame); // (StopAllCoroutines, then the delay again)
    }
    return true;
}

void SoundProps::update(float dt) {
    for (Prop& p : props_) p.cooldown -= dt;
}

void SoundProps::coroutines(World& world, float dt) {
    for (Prop& p : props_)
        if (p.emoteOff.done(world.frame, dt)) world.objects.setActive(world.area, p.emote, false);
}
