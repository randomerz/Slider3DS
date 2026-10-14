#include "game/data/json.h"
#include "test.h"

TEST(json_parses_values) {
    Json j;
    std::string err;
    bool ok = Json::parse(R"({"a": 1, "b": [true, false, null], "c": "x\"y\\z", "d": -1.5e2, "e": {"f": [0]}})", j, &err);
    check(ok, "parse: " + err);
    CHECK(j["a"].asInt() == 1);
    CHECK(j["b"].size() == 3 && j["b"].at(0).asBool() && !j["b"].at(1).asBool(true) && j["b"].at(2).isNull());
    CHECK(j["c"].asString() == "x\"y\\z");
    CHECK(j["d"].asFloat() == -150.0f);
    CHECK(j["e"]["f"].at(0).asInt(7) == 0);
}

TEST(json_missing_values_fall_back) {
    Json j;
    CHECK(Json::parse(R"({"a": [1]})", j));
    CHECK(j["missing"].isNull() && !j.has("missing") && j.has("a"));
    CHECK(j["a"].at(5).isNull() && j["a"].at(5).asInt(3) == 3);
    CHECK(j["a"].asString().empty());
}

TEST(json_rejects_malformed) {
    const char* bad[] = {R"({"a": })", "[1, 2", R"("abc)", R"({"a" 1})", "{} x", ""};
    for (const char* text : bad) {
        Json j;
        check(!Json::parse(text, j), std::string("rejects: ") + text);
    }
}
