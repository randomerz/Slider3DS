// The area's sprite sheets (game/render/sprite_sheets): loading, and what a failure reports.
#include <string>

#include "game/render/sprite_sheets.h"
#include "test.h"
#include "test_platform.h"

TEST(sprite_sheets_name_the_sheet_that_failed) {
    // A missing sheet fails the load with its name (the boot's error screen shows it), none kept.
    SpriteSheets sheets;
    std::string error;
    CHECK(sheets.load(villageArea(), &error) && error.empty());
    testFailSheet("tiles_village");
    const bool loaded = sheets.load(villageArea(), &error);
    testFailSheet("");
    check(!loaded && error.find("tiles_village") != std::string::npos, "error: " + error);
}
