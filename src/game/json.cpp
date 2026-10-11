#include "game/json.h"

#include <cstdlib>
#include <cstring>

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : p_(text.c_str()), end_(text.c_str() + text.size()) {}

    bool parseDocument(Json& out) {
        if (!parseValue(out)) return false;
        skipWs();
        return p_ == end_ || fail("trailing characters");
    }

    std::string error;

private:
    const char* p_;
    const char* end_;

    bool fail(const char* msg) {
        if (error.empty()) error = msg;
        return false;
    }

    void skipWs() {
        while (p_ < end_ && (*p_ == ' ' || *p_ == '\n' || *p_ == '\r' || *p_ == '\t')) p_++;
    }

    bool literal(const char* word) {
        size_t n = strlen(word);
        if ((size_t)(end_ - p_) < n || strncmp(p_, word, n) != 0) return fail("bad literal");
        p_ += n;
        return true;
    }

    bool parseValue(Json& v) {
        skipWs();
        if (p_ >= end_) return fail("unexpected end");
        switch (*p_) {
        case '{': return parseObject(v);
        case '[': return parseArray(v);
        case '"': v.type_ = Json::Type::String; return parseString(v.string_);
        case 't': v.type_ = Json::Type::Bool; v.bool_ = true; return literal("true");
        case 'f': v.type_ = Json::Type::Bool; v.bool_ = false; return literal("false");
        case 'n': v.type_ = Json::Type::Null; return literal("null");
        default: return parseNumber(v);
        }
    }

    bool parseNumber(Json& v) {
        char* after = nullptr;
        v.number_ = strtod(p_, &after);
        if (after == p_) return fail("bad value");
        p_ = after;
        v.type_ = Json::Type::Number;
        return true;
    }

    bool parseString(std::string& s) {
        p_++; // opening quote
        while (p_ < end_ && *p_ != '"') {
            char c = *p_++;
            if (c != '\\') { s += c; continue; }
            if (p_ >= end_) return fail("bad escape");
            char e = *p_++;
            switch (e) {
            case 'n': s += '\n'; break;
            case 't': s += '\t'; break;
            case 'r': s += '\r'; break;
            case 'b': s += '\b'; break;
            case 'f': s += '\f'; break;
            case 'u': {
                if (end_ - p_ < 4) return fail("bad \\u escape");
                unsigned cp = (unsigned)strtoul(std::string(p_, 4).c_str(), nullptr, 16);
                p_ += 4;
                // UTF-8 encode (BMP only; the exporter only escapes control characters)
                if (cp < 0x80) s += (char)cp;
                else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
                else { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
                break;
            }
            default: s += e; break; // \" \\ \/
            }
        }
        if (p_ >= end_) return fail("unterminated string");
        p_++; // closing quote
        return true;
    }

    bool parseArray(Json& v) {
        v.type_ = Json::Type::Array;
        p_++;
        skipWs();
        if (p_ < end_ && *p_ == ']') { p_++; return true; }
        for (;;) {
            v.items_.emplace_back();
            if (!parseValue(v.items_.back())) return false;
            skipWs();
            if (p_ < end_ && *p_ == ',') { p_++; continue; }
            if (p_ < end_ && *p_ == ']') { p_++; return true; }
            return fail("expected , or ]");
        }
    }

    bool parseObject(Json& v) {
        v.type_ = Json::Type::Object;
        p_++;
        skipWs();
        if (p_ < end_ && *p_ == '}') { p_++; return true; }
        for (;;) {
            skipWs();
            if (p_ >= end_ || *p_ != '"') return fail("expected key");
            v.fields_.emplace_back();
            if (!parseString(v.fields_.back().first)) return false;
            skipWs();
            if (p_ >= end_ || *p_ != ':') return fail("expected :");
            p_++;
            if (!parseValue(v.fields_.back().second)) return false;
            skipWs();
            if (p_ < end_ && *p_ == ',') { p_++; continue; }
            if (p_ < end_ && *p_ == '}') { p_++; return true; }
            return fail("expected , or }");
        }
    }
};

bool Json::parse(const std::string& text, Json& out, std::string* error) {
    JsonParser parser(text);
    out = Json();
    bool ok = parser.parseDocument(out);
    if (!ok && error) *error = parser.error;
    return ok;
}

const Json& Json::null() {
    static const Json n;
    return n;
}

const Json& Json::operator[](const char* key) const {
    for (const auto& f : fields_)
        if (f.first == key) return f.second;
    return null();
}

bool Json::has(const char* key) const {
    for (const auto& f : fields_)
        if (f.first == key) return true;
    return false;
}
