#include "game/audio/audio_bank.h"

#include <algorithm>
#include <cmath>

#include "game/data/json.h"
#include "platform/platform.h"

namespace {

float dbToGain(float db) { return std::pow(10.0f, db / 20); }

} // namespace

float SpatialModel::angle(float azimuthDeg) const {
    const float a = std::fabs(azimuthDeg);
    for (size_t i = 1; i < pan.size(); i++)
        if (a <= pan[i].first)
            return pan[i - 1].second + (pan[i].second - pan[i - 1].second) * (a - pan[i - 1].first) / (pan[i].first - pan[i - 1].first);
    return pan.back().second;
}

float AhdsrShape::release(float x) const {
    const float f = std::clamp(x, 0.0f, 1.0f) * (POINTS - 1);
    const int i = std::min((int)f, POINTS - 2);
    return db[i] + (db[i + 1] - db[i]) * (f - i);
}

namespace {

// A whole Ogg file as mono int16 at the port's rate; false if it isn't one.
bool decodeClip(const std::string& path, Clip& clip) {
    plat::Ogg* ogg = plat::openOgg(path.c_str());
    if (!ogg) return false;
    const bool ok = plat::oggChannels(ogg) == 1 && plat::oggRate(ogg) == plat::AUDIO_RATE;
    if (ok) {
        clip.pcm.resize((size_t)plat::oggFrames(ogg));
        clip.pcm.resize((size_t)plat::readOgg(ogg, clip.pcm.data(), (int)clip.pcm.size()));
    }
    plat::closeOgg(ogg);
    return ok && !clip.pcm.empty();
}

// A stream's file opens as stereo at the port's rate, long enough for its jumps.
bool checkStream(const StreamDef& def) {
    plat::Ogg* ogg = plat::openOgg(("audio/" + def.file).c_str());
    if (!ogg) return false;
    const int64_t frames = plat::oggFrames(ogg);
    bool ok = plat::oggChannels(ogg) == 2 && plat::oggRate(ogg) == plat::AUDIO_RATE && def.length > 0 && def.length <= frames;
    for (const StreamDef::Jump& j : def.jumps) ok = ok && j.at > 0 && j.at <= frames && j.to >= 0 && j.to < frames;
    plat::closeOgg(ogg);
    return ok;
}

} // namespace

bool AudioBank::load(std::string* error) {
    unload();
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        unload();
        return false;
    };
    std::string text;
    Json doc;
    if (!plat::readFile("audio/audio.json", text) || !Json::parse(text, doc, error)) return fail("can't read audio/audio.json");
    if (doc["rate"].asInt() != plat::AUDIO_RATE) return fail("audio/audio.json: not at the port's rate");
    for (const Json& k : doc["dampenCurve"].items()) {
        if (k.size() != 6) return fail("audio/audio.json: a bad dampenCurve key");
        dampenCurve_.push_back({k.at(0).asFloat(), k.at(1).asFloat(), k.at(2).asFloat(), k.at(3).asFloat(), k.at(4).asFloat(),
                                k.at(5).asFloat()});
    }
    if (dampenCurve_.size() < 2) return fail("audio/audio.json: no dampenCurve");
    const Json& shape = doc["ahdsrShape"];
    if (shape.size() != AhdsrShape::POINTS) return fail("audio/audio.json: no ahdsrShape");
    for (int i = 0; i < AhdsrShape::POINTS; i++) ahdsr_.db[i] = shape.at(i).asFloat();
    const Json& spatial = doc["spatial"];
    if (!spatial.has("gain") || spatial["pan"].size() < 2) return fail("audio/audio.json: no spatial model");
    spatial_.gain = spatial["gain"].asFloat();
    spatial_.pan.clear();
    for (const Json& p : spatial["pan"].items()) // (kept rising: a measurement's wobble near hard right flattens)
        spatial_.pan.push_back({p.at(0).asFloat(), std::max(p.at(1).asFloat(), spatial_.pan.empty() ? 45.0f : spatial_.pan.back().second)});
    const Json& eq = doc["indoorMusicEq"];
    if (!eq.has("lowHz") || !eq.has("highHz")) return fail("audio/audio.json: no indoorMusicEq");
    indoorEq_ = {eq["lowHz"].asFloat(), eq["highHz"].asFloat(), eq["lowDb"].asFloat(), eq["highDb"].asFloat()};

    for (const auto& [name, j] : doc["sounds"].members()) {
        SoundDef& s = sounds_[name];
        for (const Json& f : j["clips"].items()) {
            s.clips.emplace_back();
            if (!decodeClip("audio/" + f.asString(), s.clips.back())) return fail("audio/" + f.asString() + ": not a mono clip");
        }
        if (s.clips.empty()) return fail("audio/audio.json: sound " + name + " has no clips");
        s.gain = dbToGain(j["volume"].asFloat());
        s.pitchRandom = j["pitchRandom"].asFloat();
        s.pitchParam = j["pitchParam"].asBool();
        if (j.has("spatial")) {
            s.spatial = true;
            s.minDistance = j["spatial"].at(0).asFloat();
            s.maxDistance = j["spatial"].at(1).asFloat();
        }
        s.canPause = j["canPause"].asBool(true);
    }

    for (const auto& [event, j] : doc["voices"].members()) {
        VoiceSynth& v = voices_[event];
        const Json& vowels = j["vowels"];
        v.held = vowels.has("a");
        if (v.held) {
            for (const char* c = VoiceSynth::VOWELS; *c; c++) {
                const char key[2] = {*c, 0};
                v.clips.emplace_back();
                if (!decodeClip("audio/" + vowels[key].asString(), v.clips.back())) return fail("audio: " + event + "'s " + key + " clip");
            }
            v.loopStart = j["loop"].at(0).asInt();
            v.loopEnd = j["loop"].at(1).asInt();
            for (const Clip& c : v.clips)
                if (v.loopStart <= 0 || v.loopEnd <= v.loopStart || v.loopEnd > (int)c.pcm.size())
                    return fail("audio: " + event + "'s loop is outside its clips");
        } else {
            for (const Json& f : j["clips"].items()) {
                v.clips.emplace_back();
                if (!decodeClip("audio/" + f.asString(), v.clips.back())) return fail("audio/" + f.asString() + ": not a mono clip");
            }
            if (v.clips.empty()) return fail("audio: voice " + event + " has no clips");
        }
        v.gain = dbToGain(j["volume"].asFloat());
        v.pitched = j["pitched"].asBool();
        if (j.has("spatial")) {
            v.spatial = true;
            v.minDistance = j["spatial"].at(0).asFloat();
            v.maxDistance = j["spatial"].at(1).asFloat();
        }
    }

    for (auto [table, bus, out] : {std::make_tuple("music", Bus::Music, &music_), std::make_tuple("ambience", Bus::Ambience, &ambience_)}) {
        for (const auto& [name, j] : doc[table].members()) {
            StreamDef& s = (*out)[name];
            s.file = j["file"].asString();
            s.bus = bus;
            s.gain = dbToGain(j["volume"].asFloat());
            s.attack = j["attack"].asFloat();
            s.release = j["release"].asFloat();
            s.length = (int64_t)j["length"].asNumber();
            int64_t last = 0;
            for (const Json& jj : j["jumps"].items()) {
                StreamDef::Jump jump;
                jump.at = (int64_t)jj.at(0).asNumber();
                jump.to = (int64_t)jj.at(1).asNumber();
                if (jj.size() == 5) {
                    jump.param = jj.at(2).asString();
                    jump.min = jj.at(3).asFloat();
                    jump.max = jj.at(4).asFloat();
                } else if (jj.size() != 2) {
                    return fail("audio/audio.json: a bad jump in " + name);
                }
                if (jump.at < last) return fail("audio/audio.json: " + name + "'s jumps are out of order");
                last = jump.at;
                s.jumps.push_back(jump);
            }
            if (j.has("volumeParam")) {
                s.volumeParam = j["volumeParam"].at(0).asString();
                for (const Json& l : j["volumeParam"].at(1).items()) s.volumeLevels.push_back({l.at(0).asFloat(), l.at(1).asFloat()});
                if (s.volumeParam.empty() || s.volumeLevels.size() < 2) return fail("audio/audio.json: a bad volumeParam in " + name);
            }
            if (!checkStream(s)) return fail("audio/" + s.file + ": not a stereo stream matching its entry");
        }
    }
    return true;
}

void AudioBank::unload() {
    dampenCurve_.clear();
    ahdsr_ = {};
    indoorEq_ = {};
    spatial_ = {};
    sounds_.clear();
    music_.clear();
    ambience_.clear();
    voices_.clear();
}

const SoundDef* AudioBank::sound(std::string_view name) const {
    auto it = sounds_.find(name);
    return it == sounds_.end() ? nullptr : &it->second;
}

const StreamDef* AudioBank::music(std::string_view name) const {
    auto it = music_.find(name);
    return it == music_.end() ? nullptr : &it->second;
}

const VoiceSynth* AudioBank::voice(std::string_view event) const {
    auto it = voices_.find(event);
    return it == voices_.end() ? nullptr : &it->second;
}

const StreamDef* AudioBank::ambience(std::string_view name) const {
    auto it = ambience_.find(name);
    return it == ambience_.end() ? nullptr : &it->second;
}
