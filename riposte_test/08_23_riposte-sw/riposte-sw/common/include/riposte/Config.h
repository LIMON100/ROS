#pragma once
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Minimal INI loader: "[section]" + "key = value". Full-line comments start
// with '#' or ';'; INLINE comments are '#' only — ';' is a value character
// (fence.polygon uses it as its vertex separator, and an inline-';' rule once
// truncated the polygon to one vertex, silently disabling SM-10).
// Lookup key is "section.key". Kept dependency-free on purpose; if the config
// surface grows, swap for toml++ behind this same interface.
//
// A value that is PRESENT but does not parse as the requested type still
// returns the caller's default (fail-closed for the limit it feeds), but is
// additionally recorded in parse_failures(): a safety limit silently reverting
// to its compiled default is an operator error to surface, and the mains
// refuse to start while the list is non-empty. The legacy inline-';' comment
// ("alt_max = 15.0 ; note") lands here — under the current grammar that value
// is unparseable, and refusing startup names the offending text instead of
// flying the default.

namespace riposte {

class Config {
public:
    bool load(const std::string& path) {
        std::ifstream in(path);
        if (!in.is_open()) {
            return false;
        }
        std::string line;
        std::string section;
        while (std::getline(in, line)) {
            const std::string raw = trim(line);
            if (raw.empty() || raw.front() == '#' || raw.front() == ';') {
                continue; // blank or full-line comment
            }
            const std::string s = trim(strip_comment(raw));
            if (s.empty()) {
                continue;
            }
            if (s.front() == '[' && s.back() == ']') {
                section = trim(s.substr(1, s.size() - 2));
                continue;
            }
            const auto eq = s.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            const std::string key = trim(s.substr(0, eq));
            const std::string val = trim(s.substr(eq + 1));
            if (!key.empty()) {
                // Keys before any [section] header are stored under the bare
                // key (a "." prefix would make them unreachable).
                std::string full_key;
                if (!section.empty()) {
                    full_key = section;
                    full_key += '.';
                }
                full_key += key;
                values_[full_key] = val;
            }
        }
        return true;
    }

    std::string get_str(const std::string& key, const std::string& def) const {
        const auto it = values_.find(key);
        return it == values_.end() ? def : it->second;
    }

    double get_double(const std::string& key, double def) const {
        const auto it = values_.find(key);
        if (it == values_.end()) {
            return def;
        }
        const char* start = it->second.c_str();
        char* end = nullptr;
        errno = 0;
        const double v = std::strtod(start, &end);
        // Require the whole (already trimmed) value consumed: "8,5" must not
        // silently become 8 and feed a safety limit (G3). Reject non-finite
        // values too: strtod parses "nan"/"inf", and vmax_h=inf or
        // bat_land_frac=nan would silently disable the limit it feeds — fall
        // back to the caller's validated default instead (fail-closed).
        if (end != start && *end == '\0' && errno != ERANGE && std::isfinite(v)) {
            return v;
        }
        note_parse_failure(key, it->second, "number");
        return def;
    }

    long get_int(const std::string& key, long def) const {
        const auto it = values_.find(key);
        if (it == values_.end()) {
            return def;
        }
        const char* start = it->second.c_str();
        char* end = nullptr;
        errno = 0;
        const long v = std::strtol(start, &end, 10);
        // Require the whole (already trimmed) value consumed: "0x10" must not
        // silently become 0 and feed a safety limit (G3).
        if (end != start && *end == '\0' && errno != ERANGE) {
            return v;
        }
        note_parse_failure(key, it->second, "integer");
        return def;
    }

    bool get_bool(const std::string& key, bool def) const {
        const auto it = values_.find(key);
        if (it == values_.end()) {
            return def;
        }
        const std::string& v = it->second;
        if (v == "true" || v == "1" || v == "yes" || v == "on") {
            return true;
        }
        if (v == "false" || v == "0" || v == "no" || v == "off") {
            return false;
        }
        // Anything else keeps the historical "unrecognized => false" result
        // but is surfaced: a mistyped "ture" silently disabling a feature is
        // the same class of operator error as an unparseable number.
        note_parse_failure(key, v, "bool");
        return false;
    }

    // Keys whose value was PRESENT in the file but failed to parse as the type
    // the caller asked for (the caller's default was substituted). Populated
    // lazily by the typed getters; the flight processes check this after their
    // last config read and refuse to start while it is non-empty — a limit
    // silently reverting to its compiled default is exactly the fail-open the
    // startup gates exist to prevent.
    const std::vector<std::string>& parse_failures() const { return parse_failures_; }

    // A DURATION read straight into nanoseconds, range-checked while it is
    // still a double (review CR-02).
    //
    // The pattern this replaces — static_cast<uint64_t>(get_double(k, d) * 1e9)
    // — is a fail-open hole: converting a negative double to an unsigned type
    // yields an enormous bound that then passes every downstream "is it big
    // enough" validation, so `engage_timebox_s = -1.0` silently disabled SM-8
    // and the process started normally (reproduced). The sign and the
    // conversion range must therefore be judged BEFORE the cast, and a bad
    // value must be rejected rather than clamped: a mistyped safety duration is
    // an operator error to surface, not a number to guess at.
    //
    // Returns false with `err` set when the value is absent-and-invalid-default,
    // non-positive, or too large to represent; `out` is untouched then.
    bool get_duration_ns(const std::string& key, double def_s, uint64_t& out,
                         std::string& err) const {
        const double s = get_double(key, def_s); // already rejects NaN/Inf
        if (!(s > 0.0)) {
            err = key + " must be > 0 seconds";
            return false;
        }
        // ~292 years. Anything beyond this cannot be a real timeout, and it is
        // also where uint64_t nanoseconds stop being representable.
        constexpr double MAX_NS = 9.0e18;
        const double ns = s * 1e9;
        if (!(ns < MAX_NS)) {
            err = key + " is too large to express in nanoseconds";
            return false;
        }
        out = static_cast<uint64_t>(ns);
        return true;
    }

private:
    // Inline comments: '#' ONLY. ';' must survive inside values (see header
    // comment) — full-line ';' comments are handled before this is called.
    static std::string strip_comment(const std::string& s) {
        const auto p = s.find('#');
        return p == std::string::npos ? s : s.substr(0, p);
    }
    static std::string trim(const std::string& s) {
        const auto b = s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) {
            return "";
        }
        const auto e = s.find_last_not_of(" \t\r\n");
        return s.substr(b, e - b + 1);
    }
    // Records one present-but-unparseable value (deduplicated by key+value so
    // repeated reads of the same key do not grow the list). `mutable` because
    // the typed getters are const observers; the record is a diagnostic, not
    // logical state.
    void note_parse_failure(const std::string& key, const std::string& val,
                            const char* want) const {
        std::string entry = key;
        entry += " = '";
        entry += val;
        entry += "' (not a valid ";
        entry += want;
        entry += ")";
        for (const auto& e : parse_failures_) {
            if (e == entry) {
                return;
            }
        }
        parse_failures_.push_back(entry);
    }

    std::map<std::string, std::string> values_;
    mutable std::vector<std::string> parse_failures_;
};

} // namespace riposte
