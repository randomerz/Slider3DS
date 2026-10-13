#include "game/game_state.h"

#include <algorithm>

bool GameState::flag(const std::string& name) const {
    return std::find(flags_.begin(), flags_.end(), name) != flags_.end();
}

void GameState::setFlag(const std::string& name, bool value) {
    auto it = std::find(flags_.begin(), flags_.end(), name);
    if (value && it == flags_.end()) flags_.push_back(name);
    if (!value && it != flags_.end()) flags_.erase(it);
}

std::string GameState::str(const std::string& name) const {
    for (const auto& s : strings_)
        if (s.first == name) return s.second;
    return name;
}

void GameState::setStr(const std::string& name, const std::string& value) {
    for (auto& s : strings_)
        if (s.first == name) {
            s.second = value;
            return;
        }
    strings_.emplace_back(name, value);
}

bool GameState::hasCollectible(const std::string& name, const std::string& area) const {
    return std::any_of(collectibles_.begin(), collectibles_.end(), [&](const Collectible& c) {
        return c.name == name && (area == "None" || c.area == area);
    });
}

void GameState::setCollectible(const std::string& name, const std::string& area, bool collected) {
    auto it = std::find_if(collectibles_.begin(), collectibles_.end(),
                           [&](const Collectible& c) { return c.name == name && c.area == area; });
    if (collected && it == collectibles_.end()) collectibles_.push_back({name, area});
    if (!collected && it != collectibles_.end()) collectibles_.erase(it);
}
