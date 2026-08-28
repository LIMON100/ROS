// Unit tests for the INI Config loader. Focus on robustness (G3: an unparseable
// or missing value must fall back to the caller's default, never silently become
// zero) plus the parsing surface actually used by riposte.ini: sections, inline
// comments, whitespace trimming, and typed getters.
#include <unistd.h> // getpid, unlink

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

#include "riposte/Config.h"

using namespace riposte;

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
int checks = 0;

#define CHECK(c)                                                    \
    do {                                                            \
        ++checks;                                                   \
        if (!(c)) {                                                 \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
            return 1;                                               \
        }                                                           \
    } while (0)

std::string tmp_path(int n) {
    std::string p = "/tmp/riposte_cfg_";
    p += std::to_string(static_cast<long>(getpid()));
    p += '_';
    p += std::to_string(n);
    p += ".ini";
    return p;
}

// Writes INI content to a unique temp file and removes it on scope exit.
class TempIni {
public:
    TempIni(int n, const std::string& content) : path_(tmp_path(n)) {
        std::ofstream out(path_);
        out << content;
    }
    ~TempIni() { ::unlink(path_.c_str()); }
    TempIni(const TempIni&) = delete;
    TempIni& operator=(const TempIni&) = delete;
    TempIni(TempIni&&) = delete;
    TempIni& operator=(TempIni&&) = delete;
    const std::string& path() const { return path_; }

private:
    std::string path_;
};

int test_missing_file_returns_false() {
    Config c;
    CHECK(!c.load("/tmp/riposte_nonexistent_cfg_zzzz.ini"));
    // Getters on an unloaded config still return defaults.
    CHECK(c.get_str("a.b", "DEF") == "DEF");
    CHECK(c.get_int("a.b", 42) == 42);
    return 0;
}

int test_basic_parse() {
    const TempIni f(1, "[obc]\nsource = guidance\ncmd_socket = /tmp/x.sock\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_str("obc.source", "") == "guidance");
    CHECK(c.get_str("obc.cmd_socket", "") == "/tmp/x.sock");
    return 0;
}

int test_missing_key_defaults() {
    const TempIni f(2, "[obc]\nsource = guidance\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_str("obc.absent", "DEF") == "DEF");
    CHECK(c.get_int("obc.absent", 7) == 7);
    CHECK(c.get_double("obc.absent", 1.5) == 1.5);
    CHECK(c.get_bool("obc.absent", true));
    CHECK(!c.get_bool("obc.absent", false));
    return 0;
}

int test_typed_numbers() {
    const TempIni f(3, "[lim]\nvmax = 5.5\ncount = 20\nneg = -3\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_double("lim.vmax", 0.0) == 5.5);
    CHECK(c.get_int("lim.count", 0) == 20);
    CHECK(c.get_int("lim.neg", 0) == -3);
    return 0;
}

int test_malformed_number_falls_back() {
    // G3: a non-numeric value must yield the default, NOT 0.
    const TempIni f(4, "[lim]\nvmax = abc\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_double("lim.vmax", 9.9) == 9.9);
    CHECK(c.get_int("lim.vmax", 7) == 7);
    return 0;
}

int test_empty_value_falls_back() {
    // Key present but value empty: string is "" (key exists), numbers fall back.
    const TempIni f(5, "[lim]\nvmax =\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_str("lim.vmax", "X").empty());
    CHECK(c.get_double("lim.vmax", 3.3) == 3.3);
    CHECK(c.get_int("lim.vmax", 4) == 4);
    return 0;
}

int test_inline_comments() {
    // Inline comments are '#' ONLY. ';' is a value character (fence.polygon's
    // vertex separator) — an inline-';' rule once truncated the polygon to one
    // vertex and silently disabled SM-10, so this pins the grammar.
    const TempIni f(6, "[obc]\ntoken = secret # trailing\nport = 14540 ; note\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_str("obc.token", "") == "secret");
    CHECK(c.get_str("obc.port", "") == "14540 ; note"); // ';' kept verbatim
    CHECK(c.get_int("obc.port", 7) == 7); // not a clean number -> default (G3)
    return 0;
}

int test_semicolon_separated_value_survives() {
    // The documented fence.polygon format must round-trip through the loader
    // whole — this is the loader half of the SM-10 end-to-end path.
    const TempIni f(14, "[fence]\npolygon = 37.1,127.1; 37.2,127.1; 37.2,127.2\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_str("fence.polygon", "") == "37.1,127.1; 37.2,127.1; 37.2,127.2");
    return 0;
}

int test_full_line_semicolon_comment_ignored() {
    // riposte.ini uses full-line ';' comments — those still parse as comments.
    const TempIni f(15, "; full-line comment\n  ; indented comment\n[s]\nk = v\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_str("s.k", "") == "v");
    return 0;
}

int test_whitespace_and_section_trim() {
    const TempIni f(7, "[  obc  ]\n   key   =   val val   \n");
    Config c;
    CHECK(c.load(f.path()));
    // Section and key ends trimmed; internal spaces in the value preserved.
    CHECK(c.get_str("obc.key", "") == "val val");
    return 0;
}

int test_bool_variants() {
    const TempIni f(8, "[fl]\na=true\nb=1\nc=yes\nd=on\ne=false\ng=nonsense\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_bool("fl.a", false));
    CHECK(c.get_bool("fl.b", false));
    CHECK(c.get_bool("fl.c", false));
    CHECK(c.get_bool("fl.d", false));
    CHECK(!c.get_bool("fl.e", true));
    CHECK(!c.get_bool("fl.g", true)); // unrecognized => false
    return 0;
}

int test_blank_comment_and_no_equals_lines_ignored() {
    const TempIni f(9, "# a comment\n; another\n\n[s]\nno equals here\nk = v\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_str("s.k", "") == "v");
    CHECK(c.get_str("s.no equals here", "MISS") == "MISS"); // not stored
    return 0;
}

int test_duplicate_key_last_wins() {
    const TempIni f(10, "[s]\nk = 1\nk = 2\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_int("s.k", 0) == 2);
    return 0;
}

int test_partially_numeric_falls_back() {
    // G3: a partially-numeric value must yield the default, not its numeric
    // prefix ("0x10" as int would otherwise silently become 0).
    const TempIni f(11, "[lim]\nhex = 0x10\nexp = 1e3\npair = 8,5\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_int("lim.hex", 7) == 7);           // base-10 strtol stops at 'x'
    CHECK(c.get_int("lim.exp", 7) == 7);           // 'e3' not consumed by strtol
    CHECK(c.get_double("lim.exp", 0.0) == 1000.0); // strtod parses 1e3 fully
    CHECK(c.get_int("lim.pair", 7) == 7);
    CHECK(c.get_double("lim.pair", 9.9) == 9.9);
    return 0;
}

int test_nonfinite_number_falls_back() {
    // strtod happily parses "nan"/"inf", but a non-finite value feeding a
    // safety limit (vmax_h=inf, bat_land_frac=nan) would disable that limit —
    // get_double must fall back to the caller's validated default instead.
    const TempIni f(16, "[lim]\na = nan\nb = inf\nc = -inf\nd = NaN\ne = 1e999\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_double("lim.a", 1.5) == 1.5);
    CHECK(c.get_double("lim.b", 2.5) == 2.5);
    CHECK(c.get_double("lim.c", 3.5) == 3.5);
    CHECK(c.get_double("lim.d", 4.5) == 4.5);
    CHECK(c.get_double("lim.e", 5.5) == 5.5); // overflow -> ERANGE -> default
    return 0;
}

// G16.6 deviation: one scenario walked end-to-end; each CHECK pins one property
// (G16.6) NOLINTNEXTLINE(readability-function-size)
int test_parse_failures_surface_present_bad_values() {
    // A value that is PRESENT but unparseable still falls back to the caller's
    // default (fail-closed for the limit it feeds), but must additionally be
    // RECORDED: the compiled default is in range, so downstream validation
    // cannot see the substitution, and the mains refuse startup on a non-empty
    // list. The legacy inline-';' comment is the canonical trigger.
    const TempIni f(17,
                    "[lim]\n"
                    "vmax = 15.0 ; trial\n"
                    "count = 0x10\n"
                    "flag = ture\n"
                    "ok = 2.0\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.parse_failures().empty()); // populated lazily, by the typed getters
    CHECK(c.get_double("lim.vmax", 9.9) == 9.9);
    CHECK(c.get_int("lim.count", 7) == 7);
    CHECK(!c.get_bool("lim.flag", true));
    CHECK(c.get_double("lim.ok", 0.0) == 2.0); // a good value records nothing
    CHECK(c.get_int("lim.absent", 1) == 1);    // an absent key records nothing
    CHECK(c.get_str("lim.vmax", "").find(';') != std::string::npos); // raw kept
    CHECK(c.parse_failures().size() == 3);
    // Re-reading the same bad key does not grow the list (deduplicated).
    CHECK(c.get_double("lim.vmax", 9.9) == 9.9);
    CHECK(c.parse_failures().size() == 3);
    // The entry names the key and the offending text, so the operator can fix
    // the file from the log alone.
    CHECK(c.parse_failures()[0].find("lim.vmax") != std::string::npos);
    CHECK(c.parse_failures()[0].find("15.0 ; trial") != std::string::npos);
    return 0;
}

int test_sectionless_key_readable() {
    // Keys before the first [section] header are stored under the bare key.
    const TempIni f(12, "top = 3\n[s]\nk = 1\n");
    Config c;
    CHECK(c.load(f.path()));
    CHECK(c.get_int("top", 0) == 3);
    CHECK(c.get_int("s.k", 0) == 1);
    return 0;
}

// Durations must be range-checked BEFORE the cast to unsigned nanoseconds
// (review CR-02). The reproduced failure: engage_timebox_s = -1.0 became a huge
// uint64_t, sailed through the "is it big enough" validation, and disabled SM-8
// while the process started normally.
// NaN falls back to the DEFAULT in get_double, so a sane default stands and a
// negative default is still rejected.
int test_duration_uses_default_for_nonfinite() {
    const TempIni f(14, "[safety]\nnan_v = nan\n");
    Config c;
    CHECK(c.load(f.path()));
    uint64_t ns = 0;
    std::string err;
    CHECK(c.get_duration_ns("safety.nan_v", 2.0, ns, err));
    CHECK(ns == 2'000'000'000ULL);
    CHECK(!c.get_duration_ns("safety.nan_v", -2.0, ns, err));
    return 0;
}

int test_duration_rejects_before_the_unsigned_cast() {
    const TempIni f(13,
                    "[safety]\n"
                    "neg = -1.0\n"
                    "zero = 0\n"
                    "huge = 1e30\n"
                    "ok = 2.5\n");
    Config c;
    CHECK(c.load(f.path()));
    uint64_t ns = 0;
    std::string err;

    CHECK(!c.get_duration_ns("safety.neg", 1.0, ns, err));
    CHECK(!err.empty());
    CHECK(!c.get_duration_ns("safety.zero", 1.0, ns, err));
    CHECK(!c.get_duration_ns("safety.huge", 1.0, ns, err));

    ns = 0;
    CHECK(c.get_duration_ns("safety.ok", 1.0, ns, err));
    CHECK(ns == 2'500'000'000ULL);
    // A missing key takes the default rather than failing.
    CHECK(c.get_duration_ns("safety.absent", 0.5, ns, err));
    CHECK(ns == 500'000'000ULL);
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_missing_file_returns_false();
    rc = rc != 0 ? rc : test_basic_parse();
    rc = rc != 0 ? rc : test_missing_key_defaults();
    rc = rc != 0 ? rc : test_typed_numbers();
    rc = rc != 0 ? rc : test_malformed_number_falls_back();
    rc = rc != 0 ? rc : test_empty_value_falls_back();
    rc = rc != 0 ? rc : test_inline_comments();
    rc = rc != 0 ? rc : test_semicolon_separated_value_survives();
    rc = rc != 0 ? rc : test_full_line_semicolon_comment_ignored();
    rc = rc != 0 ? rc : test_whitespace_and_section_trim();
    rc = rc != 0 ? rc : test_bool_variants();
    rc = rc != 0 ? rc : test_blank_comment_and_no_equals_lines_ignored();
    rc = rc != 0 ? rc : test_duplicate_key_last_wins();
    rc = rc != 0 ? rc : test_partially_numeric_falls_back();
    rc = rc != 0 ? rc : test_nonfinite_number_falls_back();
    rc = rc != 0 ? rc : test_parse_failures_surface_present_bad_values();
    rc = rc != 0 ? rc : test_sectionless_key_readable();
    rc = rc != 0 ? rc : test_duration_rejects_before_the_unsigned_cast();
    rc = rc != 0 ? rc : test_duration_uses_default_for_nonfinite();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_config: %d checks passed\n", checks);
    return 0;
}
