// The save profile's state the game reads and writes: bool flags (SaveProfile.GetBool / SetBool),
// strings (GetString / SetString; dialogue's <var> tags) and collected collectibles (PlayerInventory).
// The profile file (game/save/profile) keeps it with the grid and the player.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class GameState {
public:
    // SaveProfile's constructor: the profile name is the cat's ("Cat" / "CatUpper" in dialogue).
    void newProfile(const std::string& profileName);

    bool flag(std::string_view name) const; // (no string built for a literal)
    void setFlag(const std::string& name, bool value);

    // SaveProfile.GetString: an unset string reads as its name ("Cat" until a profile names the cat).
    std::string str(const std::string& name) const;
    void setStr(const std::string& name, const std::string& value);

    // PlayerInventory.Contains(name, area): area "None" matches any area.
    bool hasCollectible(const std::string& name, const std::string& area) const;
    void setCollectible(const std::string& name, const std::string& area, bool collected);

    // Replaces everything with `o`'s (a load). The version moves past both, so checks keyed on it
    // re-run.
    void assign(const GameState& o);

    // Bumped by every change, so per-frame checks (NPC conditionals) can skip unchanged frames.
    uint32_t version() const { return version_; }

    struct Collectible {
        std::string name, area;
    };
    // Everything set, in the order it was (for saving).
    const std::vector<std::string>& flags() const { return flags_; }
    const std::vector<std::pair<std::string, std::string>>& strings() const { return strings_; }
    const std::vector<Collectible>& collectibles() const { return collectibles_; }

private:
    std::vector<std::string> flags_; // set to true
    std::vector<std::pair<std::string, std::string>> strings_;
    std::vector<Collectible> collectibles_;
    uint32_t version_ = 0;
};
