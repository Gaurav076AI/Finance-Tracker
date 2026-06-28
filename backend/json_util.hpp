#pragma once

// Lightweight JSON helpers for request parsing and response building.
// Avoids external dependencies; sufficient for this API's flat objects/arrays.

#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ft {

// Escape a string for safe inclusion inside JSON double quotes.
inline std::string jsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

// Skip whitespace starting at index i.
inline void skipWs(const std::string& json, size_t& i) {
    while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) {
        ++i;
    }
}

// Parse a JSON string value; returns empty string if parsing fails.
inline std::string parseJsonString(const std::string& json, size_t& i) {
    skipWs(json, i);
    if (i >= json.size() || json[i] != '"') return "";
    ++i;
    std::string out;
    while (i < json.size()) {
        char c = json[i++];
        if (c == '"') break;
        if (c == '\\' && i < json.size()) {
            char e = json[i++];
            if (e == '"') out += '"';
            else if (e == '\\') out += '\\';
            else if (e == 'n') out += '\n';
            else if (e == 't') out += '\t';
            else out += e;
        } else {
            out += c;
        }
    }
    return out;
}

// Parse number (int) from JSON.
inline bool parseJsonInt(const std::string& json, size_t& i, long long& value) {
    skipWs(json, i);
    size_t start = i;
    if (i < json.size() && (json[i] == '-' || json[i] == '+')) ++i;
    while (i < json.size() && std::isdigit(static_cast<unsigned char>(json[i]))) ++i;
    if (start == i) return false;
    try {
        value = std::stoll(json.substr(start, i - start));
        return true;
    } catch (...) {
        return false;
    }
}

// Parse boolean literal true/false.
inline bool parseJsonBool(const std::string& json, size_t& i, bool& value) {
    skipWs(json, i);
    if (json.compare(i, 4, "true") == 0) { i += 4; value = true; return true; }
    if (json.compare(i, 5, "false") == 0) { i += 5; value = false; return true; }
    return false;
}

// Extract top-level string fields from a flat JSON object: O(n) over body length.
inline std::unordered_map<std::string, std::string> parseFlatObject(const std::string& json) {
    std::unordered_map<std::string, std::string> result;
    size_t i = 0;
    skipWs(json, i);
    if (i >= json.size() || json[i] != '{') return result;
    ++i;

    while (i < json.size()) {
        skipWs(json, i);
        if (i < json.size() && json[i] == '}') break;

        std::string key = parseJsonString(json, i);
        skipWs(json, i);
        if (i < json.size() && json[i] == ':') ++i;

        skipWs(json, i);
        if (i >= json.size()) break;

        if (json[i] == '"') {
            result[key] = parseJsonString(json, i);
        } else if (json[i] == 't' || json[i] == 'f') {
            bool b = false;
            if (parseJsonBool(json, i, b)) {
                result[key] = b ? "true" : "false";
            }
        } else if (json[i] == '-' || std::isdigit(static_cast<unsigned char>(json[i]))) {
            long long num = 0;
            if (parseJsonInt(json, i, num)) {
                result[key] = std::to_string(num);
            }
        } else if (json[i] == 'n') {
            i += 4; // null
            result[key] = "";
        }

        skipWs(json, i);
        if (i < json.size() && json[i] == ',') ++i;
    }
    return result;
}

inline bool fieldBool(const std::unordered_map<std::string, std::string>& m,
                      const std::string& key, bool defaultValue = false) {
    auto it = m.find(key);
    if (it == m.end()) return defaultValue;
    return it->second == "true" || it->second == "1";
}

inline int fieldInt(const std::unordered_map<std::string, std::string>& m,
                    const std::string& key, int defaultValue = 0) {
    auto it = m.find(key);
    if (it == m.end() || it->second.empty()) return defaultValue;
    try { return std::stoi(it->second); } catch (...) { return defaultValue; }
}

inline std::string fieldStr(const std::unordered_map<std::string, std::string>& m,
                            const std::string& key, const std::string& defaultValue = "") {
    auto it = m.find(key);
    return it == m.end() ? defaultValue : it->second;
}

} // namespace ft
