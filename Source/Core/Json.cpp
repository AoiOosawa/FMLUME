#include "Core/Json.h"
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace fmlume
{
const Json* Json::find (const std::string& key) const
{
    for (auto& m : members)
        if (m.first == key) return &m.second;
    return nullptr;
}

Json& Json::set (const std::string& key, Json value)
{
    for (auto& m : members)
        if (m.first == key) { m.second = std::move (value); return m.second; }
    members.emplace_back (key, std::move (value));
    return members.back().second;
}

Json& Json::push (Json value)
{
    items.push_back (std::move (value));
    return items.back();
}

bool Json::remove (const std::string& key)
{
    for (size_t i = 0; i < members.size(); ++i)
        if (members[i].first == key) { members.erase (members.begin() + (long) i); return true; }
    return false;
}

namespace
{
// strtod honours the process locale. The token is already known to be JSON-shaped ('.' as the
// decimal point), so when the locale uses another separator the token is re-spelled for it.
double parseDoubleC (std::string token)
{
    char* end = nullptr;
    double v = std::strtod (token.c_str(), &end);
    if (end != nullptr && *end == 0) return v;
    const char* point = std::localeconv()->decimal_point;
    const size_t dot = token.find ('.');
    if (dot != std::string::npos && point != nullptr && point[0] != 0 && point[0] != '.')
    {
        token.replace (dot, 1, point);
        v = std::strtod (token.c_str(), &end);
    }
    return v;
}
} // namespace

namespace
{
struct Parser
{
    const std::string& s;
    size_t pos = 0;
    std::string error;
    static constexpr int kMaxDepth = 64;
    // Resource limits: a preset has a few thousand values. These keep a hostile file from
    // exhausting memory or stalling the duplicate-key check (which is linear per member).
    static constexpr size_t kMaxNodes = 200000, kMaxMembers = 2048;
    size_t nodes = 0;

    explicit Parser (const std::string& text) : s (text) {}

    bool fail (const char* msg)
    {
        if (error.empty())
        {
            int line = 1;
            for (size_t i = 0; i < pos && i < s.size(); ++i) line += s[i] == '\n';
            error = "line " + std::to_string (line) + ": " + msg;
        }
        return false;
    }
    void skipWs()
    {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) ++pos;
    }
    bool literal (const char* word)
    {
        size_t n = 0;
        while (word[n] != 0) ++n;
        if (s.compare (pos, n, word) != 0) return fail ("invalid literal");
        pos += n;
        return true;
    }
    bool parseString (std::string& out)
    {
        if (pos >= s.size() || s[pos] != '"') return fail ("expected string");
        ++pos;
        out.clear();
        while (pos < s.size())
        {
            const unsigned char c = (unsigned char) s[pos++];
            if (c == '"') return true;
            if (c < 0x20) return fail ("control character in string");
            if (c != '\\') { out.push_back ((char) c); continue; }
            if (pos >= s.size()) break;
            const char e = s[pos++];
            switch (e)
            {
                case '"': out.push_back ('"'); break;
                case '\\': out.push_back ('\\'); break;
                case '/': out.push_back ('/'); break;
                case 'b': out.push_back ('\b'); break;
                case 'f': out.push_back ('\f'); break;
                case 'n': out.push_back ('\n'); break;
                case 'r': out.push_back ('\r'); break;
                case 't': out.push_back ('\t'); break;
                case 'u':
                {
                    auto hex4 = [&] (unsigned& cp) {
                        if (pos + 4 > s.size()) return false;
                        cp = 0;
                        for (int i = 0; i < 4; ++i)
                        {
                            const char h = s[pos++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (unsigned) (h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (unsigned) (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (unsigned) (h - 'A' + 10);
                            else return false;
                        }
                        return true;
                    };
                    unsigned cp = 0;
                    if (! hex4 (cp)) return fail ("bad \\u escape");
                    if (cp >= 0xD800 && cp <= 0xDBFF)
                    {
                        unsigned lo = 0;
                        if (pos + 2 > s.size() || s[pos] != '\\' || s[pos + 1] != 'u') return fail ("bad surrogate pair");
                        pos += 2;
                        if (! hex4 (lo) || lo < 0xDC00 || lo > 0xDFFF) return fail ("bad surrogate pair");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    else if (cp >= 0xDC00 && cp <= 0xDFFF) return fail ("bad surrogate pair");
                    if (cp < 0x80) out.push_back ((char) cp);
                    else if (cp < 0x800) { out.push_back ((char) (0xC0 | (cp >> 6))); out.push_back ((char) (0x80 | (cp & 0x3F))); }
                    else if (cp < 0x10000) { out.push_back ((char) (0xE0 | (cp >> 12))); out.push_back ((char) (0x80 | ((cp >> 6) & 0x3F))); out.push_back ((char) (0x80 | (cp & 0x3F))); }
                    else { out.push_back ((char) (0xF0 | (cp >> 18))); out.push_back ((char) (0x80 | ((cp >> 12) & 0x3F))); out.push_back ((char) (0x80 | ((cp >> 6) & 0x3F))); out.push_back ((char) (0x80 | (cp & 0x3F))); }
                    break;
                }
                default: return fail ("bad escape");
            }
        }
        return fail ("unterminated string");
    }
    bool parseNumber (Json& out)
    {
        const size_t start = pos;
        if (pos < s.size() && s[pos] == '-') ++pos;
        if (pos >= s.size() || s[pos] < '0' || s[pos] > '9') return fail ("invalid number");
        if (s[pos] == '0') ++pos;
        else while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
        if (pos < s.size() && s[pos] == '.')
        {
            ++pos;
            if (pos >= s.size() || s[pos] < '0' || s[pos] > '9') return fail ("invalid number");
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E'))
        {
            ++pos;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
            if (pos >= s.size() || s[pos] < '0' || s[pos] > '9') return fail ("invalid number");
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
        }
        const double v = parseDoubleC (s.substr (start, pos - start));
        if (! std::isfinite (v)) return fail ("number out of range");
        out = Json (v);
        return true;
    }
    bool parseValue (Json& out, int depth)
    {
        if (depth > kMaxDepth) return fail ("nesting too deep");
        if (++nodes > kMaxNodes) return fail ("too many values");
        skipWs();
        if (pos >= s.size()) return fail ("unexpected end of input");
        const char c = s[pos];
        if (c == '{')
        {
            ++pos;
            out = Json::object();
            skipWs();
            if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
            for (;;)
            {
                skipWs();
                std::string key;
                if (! parseString (key)) return false;
                skipWs();
                if (pos >= s.size() || s[pos] != ':') return fail ("expected ':'");
                ++pos;
                Json v;
                if (! parseValue (v, depth + 1)) return false;
                if (out.members.size() >= kMaxMembers) return fail ("too many keys in one object");
                if (out.find (key) != nullptr) return fail ("duplicate object key");
                out.members.emplace_back (std::move (key), std::move (v));
                skipWs();
                if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
                if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
                return fail ("expected ',' or '}'");
            }
        }
        if (c == '[')
        {
            ++pos;
            out = Json::array();
            skipWs();
            if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
            for (;;)
            {
                Json v;
                if (! parseValue (v, depth + 1)) return false;
                out.items.push_back (std::move (v));
                skipWs();
                if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
                if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
                return fail ("expected ',' or ']'");
            }
        }
        if (c == '"')
        {
            std::string str;
            if (! parseString (str)) return false;
            out = Json (std::move (str));
            return true;
        }
        if (c == 't') { if (! literal ("true")) return false; out = Json (true); return true; }
        if (c == 'f') { if (! literal ("false")) return false; out = Json (false); return true; }
        if (c == 'n') { if (! literal ("null")) return false; out = Json(); return true; }
        return parseNumber (out);
    }
};

void dumpString (const std::string& s, std::string& out)
{
    out.push_back ('"');
    for (const char ch : s)
    {
        const unsigned char c = (unsigned char) ch;
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char buf[8]; std::snprintf (buf, sizeof (buf), "\\u%04x", c); out += buf; }
                else out.push_back (ch);
        }
    }
    out.push_back ('"');
}

void dumpValue (const Json& j, int indent, int level, std::string& out)
{
    auto newline = [&] (int lvl) {
        if (indent <= 0) return;
        out.push_back ('\n');
        out.append ((size_t) (indent * lvl), ' ');
    };
    switch (j.type)
    {
        case Json::Type::Null: out += "null"; break;
        case Json::Type::Bool: out += j.boolean ? "true" : "false"; break;
        case Json::Type::Number:
        {
            char buf[40];
            if (! std::isfinite (j.number)) { out += "null"; break; }
            if (j.number == std::floor (j.number) && std::fabs (j.number) < 1.0e15) std::snprintf (buf, sizeof (buf), "%.0f", j.number);
            else std::snprintf (buf, sizeof (buf), j.isFloat ? "%.9g" : "%.17g", j.number);
            // snprintf follows the process locale (a host may have set a comma-decimal one); JSON needs '.'.
            for (char* c = buf; *c != 0; ++c) if (*c == ',') *c = '.';
            out += buf;
            break;
        }
        case Json::Type::String: dumpString (j.string, out); break;
        case Json::Type::Array:
        {
            if (j.items.empty()) { out += "[]"; break; }
            bool scalar = true;
            for (auto& v : j.items) scalar = scalar && ! v.isArray() && ! v.isObject();
            out.push_back ('[');
            for (size_t i = 0; i < j.items.size(); ++i)
            {
                if (i > 0) out += scalar && indent > 0 ? ", " : ",";
                if (! scalar) newline (level + 1);
                dumpValue (j.items[i], indent, level + 1, out);
            }
            if (! scalar) newline (level);
            out.push_back (']');
            break;
        }
        case Json::Type::Object:
        {
            if (j.members.empty()) { out += "{}"; break; }
            out.push_back ('{');
            for (size_t i = 0; i < j.members.size(); ++i)
            {
                if (i > 0) out.push_back (',');
                newline (level + 1);
                dumpString (j.members[i].first, out);
                out += indent > 0 ? ": " : ":";
                dumpValue (j.members[i].second, indent, level + 1, out);
            }
            newline (level);
            out.push_back ('}');
            break;
        }
    }
}
} // namespace

bool Json::parse (const std::string& text, Json& out, std::string* error)
{
    if (text.size() > kMaxJsonBytes)
    {
        if (error) *error = "input too large";
        return false;
    }
    Parser p (text);
    Json result;
    if (! p.parseValue (result, 0))
    {
        if (error) *error = p.error;
        return false;
    }
    p.skipWs();
    if (p.pos != text.size())
    {
        p.fail ("unexpected trailing characters");
        if (error) *error = p.error;
        return false;
    }
    out = std::move (result);
    return true;
}

std::string Json::dump (int indent) const
{
    std::string out;
    dumpValue (*this, indent, 0, out);
    return out;
}
} // namespace fmlume
