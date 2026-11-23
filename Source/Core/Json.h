// Minimal JSON value / parser / writer (non-realtime). Object keys keep insertion order.
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace fmlume
{
class Json
{
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json (bool v) : type (Type::Bool), boolean (v) {}
    Json (double v) : type (Type::Number), number (v) {}
    Json (float v) : type (Type::Number), number ((double) v), isFloat (true) {}
    Json (int v) : type (Type::Number), number ((double) v) {}
    Json (const char* v) : type (Type::String), string (v) {}
    Json (std::string v) : type (Type::String), string (std::move (v)) {}
    static Json array() { Json j; j.type = Type::Array; return j; }
    static Json object() { Json j; j.type = Type::Object; return j; }

    bool isNull() const { return type == Type::Null; }
    bool isBool() const { return type == Type::Bool; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }

    const Json* find (const std::string& key) const;
    Json& set (const std::string& key, Json value); // replaces an existing key
    Json& push (Json value);
    bool remove (const std::string& key);

    // Returns false and fills `error` ("line L: message") on malformed input.
    // Refuses inputs over kMaxJsonBytes, more than 200k values, more than 2048 keys per object
    // or nesting deeper than 64 (resource limits against hostile files).
    static constexpr size_t kMaxJsonBytes = 8u * 1024u * 1024u;
    static bool parse (const std::string& text, Json& out, std::string* error = nullptr);
    std::string dump (int indent = 2) const;

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    bool isFloat = false; // written with float round-trip precision
    std::string string;
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> members;
};
} // namespace fmlume
