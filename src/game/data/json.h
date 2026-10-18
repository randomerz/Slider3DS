// Minimal JSON DOM for loading exported data (no exceptions; parse returns false on error).
#pragma once

#include <string>
#include <utility>
#include <vector>

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    static bool parse(const std::string& text, Json& out, std::string* error = nullptr);

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }

    bool asBool(bool fallback = false) const { return type_ == Type::Bool ? bool_ : fallback; }
    double asNumber(double fallback = 0.0) const { return type_ == Type::Number ? number_ : fallback; }
    int asInt(int fallback = 0) const { return type_ == Type::Number ? (int)number_ : fallback; }
    float asFloat(float fallback = 0.0f) const { return type_ == Type::Number ? (float)number_ : fallback; }
    const std::string& asString() const { return string_; }

    // Arrays
    size_t size() const { return items_.size(); }
    const Json& at(size_t i) const { return i < items_.size() ? items_[i] : null(); }
    const std::vector<Json>& items() const { return items_; }

    // Objects: missing keys yield a Null value.
    const Json& operator[](const char* key) const;
    bool has(const char* key) const;
    const std::vector<std::pair<std::string, Json>>& members() const { return fields_; } // in file order

private:
    static const Json& null();

    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Json> items_;                          // array elements
    std::vector<std::pair<std::string, Json>> fields_; // object members, in file order

    friend class JsonParser;
};
