// The World's ongoing sounds:
//   footsteps: the player's Animator's "Step" events (PlayerRun's, twice a cycle) are
//     MaterialBasedEmitter.Step — the surface under the player (its STile's material map, the
//     house's while inside, the world's off any tile; the mapping's fallback off any material tile)
//     as "Footstep <surface>", at the player; none while the player is held (!Player.GetCanMove);
//   ambiences: DistanceBasedAmbience — each plays from its scene's start, its global parameter set
//     to the player's distance to the nearest active node (the lowest of the ones sharing an
//     ambience; sent when it changes), and stops (the parameter back to its default) as the scene ends.
#pragma once

#include <string>
#include <vector>

class World;

class WorldSounds {
public:
    void reset() { *this = WorldSounds(); }
    void start(World& world);  // DistanceBasedAmbience.Start
    void update(World& world); // after the Animator: the frame's steps; then the ambiences' parameters
    void end(World& world);    // the scene unloads (OnDestroy)

    // The footstep surface at (x, y) as the player would hear it (Area::footsteps' names).
    static const std::string& surfaceAt(const World& world, float x, float y);

private:
    void sendParameters(World& world);
    std::vector<float> sent_; // per ambience: the parameter last sent (-1: none yet)
    bool started_ = false;
};
