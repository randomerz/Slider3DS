// The save-profile state the game reads and writes: bool flags (SaveProfile.GetBool / SetBool),
// strings (GetString / SetString; dialogue's <var> tags) and collected collectibles (PlayerInventory). A stand-in with the same queries until the save
// profile exists (phase 4), which will add items, the grid and the player's position.
#pragma once

#include <string>
#include <utility>
#include <vector>

class GameState {
public:
    bool flag(const std::string& name) const;
    void setFlag(const std::string& name, bool value);

    // SaveProfile.GetString: an unset string reads as its name ("Cat" until a profile names the cat).
    std::string str(const std::string& name) const;
    void setStr(const std::string& name, const std::string& value);

    // PlayerInventory.Contains(name, area): area "None" matches any area.
    bool hasCollectible(const std::string& name, const std::string& area) const;
    void setCollectible(const std::string& name, const std::string& area, bool collected);

private:
    struct Collectible {
        std::string name, area;
    };
    std::vector<std::string> flags_; // set to true
    std::vector<std::pair<std::string, std::string>> strings_;
    std::vector<Collectible> collectibles_;
};
