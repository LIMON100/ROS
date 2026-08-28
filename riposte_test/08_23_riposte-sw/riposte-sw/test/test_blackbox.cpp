// Unit tests for the supervisor's size-bounded blackbox rotation (G11.2). Pure
// filesystem logic — no shm, no buses. Exercises numbered retention: the active
// file rotates to base.1..base.N at the size cap and base.(N+1) is discarded.
#include "Blackbox.h"

#include <cstdio>
#include <string>

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

constexpr const char* BASE = "/tmp/riposte_bb_test.jsonl";

// Archive path base.N.
std::string arc(int n) {
    return std::string(BASE) + "." + std::to_string(n);
}

bool exists(const std::string& p) {
    const FilePtr f(std::fopen(p.c_str(), "r"),
                    &std::fclose); // FilePtr owns/closes it (RAII)
    return f != nullptr;
}

long file_size(const std::string& p) {
    const FilePtr f(std::fopen(p.c_str(), "r"), &std::fclose);
    if (f == nullptr) {
        return -1;
    }
    (void)std::fseek(f.get(), 0, SEEK_END);
    return std::ftell(f.get());
}

void cleanup() {
    (void)std::remove(BASE);
    for (int i = 1; i <= 12; ++i) {
        (void)std::remove(arc(i).c_str());
    }
}

// 21-byte line (20 chars + newline).
constexpr const char* LINE = "0123456789ABCDEFGHIJ\n";
constexpr int LINE_LEN = 21;

int test_rotation_and_retention() {
    cleanup();
    {
        Blackbox bb(BASE, /*max_bytes=*/100, /*keep=*/3);
        CHECK(bb.ok());
        // 28 lines * 21 B = 588 B. Rotates every 5 lines (105 >= 100), so 5
        // rotations occur, leaving 3 lines (63 B) in the fresh base file.
        for (int i = 0; i < 28; ++i) {
            bb.write_line(LINE, LINE_LEN);
        }
    } // bb closed

    CHECK(exists(BASE));   // active file present
    CHECK(exists(arc(1))); // newest archive
    CHECK(exists(arc(2)));
    CHECK(exists(arc(3)));                     // oldest kept archive (keep=3)
    CHECK(!exists(arc(4)));                    // beyond retention: discarded
    CHECK(file_size(BASE) == 3L * LINE_LEN);   // remainder after last rotation
    CHECK(file_size(arc(1)) == 5L * LINE_LEN); // each archive holds one cap's worth
    cleanup();
    return 0;
}

int test_append_preserves_existing() {
    cleanup();
    {
        Blackbox bb(BASE, /*max_bytes=*/0, /*keep=*/3); // 0 = no rotation
        bb.write_line(LINE, LINE_LEN);
        bb.write_line(LINE, LINE_LEN);
    }
    // Reopen: "a" mode must preserve the two existing lines and keep counting
    // from the real file size (not reset to 0).
    {
        Blackbox bb(BASE, /*max_bytes=*/0, /*keep=*/3);
        CHECK(bb.ok());
        bb.write_line(LINE, LINE_LEN);
    }
    CHECK(file_size(BASE) == 3L * LINE_LEN); // appended, not truncated
    CHECK(!exists(arc(1)));                  // no rotation when max_bytes == 0
    cleanup();
    return 0;
}

int test_keep_zero_truncates() {
    cleanup();
    {
        Blackbox bb(BASE, /*max_bytes=*/40, /*keep=*/0); // keep 0 archives
        for (int i = 0; i < 10; ++i) {
            bb.write_line(LINE, LINE_LEN);
        }
    }
    CHECK(exists(BASE));    // active file present
    CHECK(!exists(arc(1))); // no archives retained at all
    CHECK(file_size(BASE) <= 40);
    cleanup();
    return 0;
}

int test_write_line_reports_success() {
    // P2-02: a good write returns true and ok() stays true.
    cleanup();
    Blackbox bb(BASE, /*max_bytes=*/0, /*keep=*/0);
    CHECK(bb.ok());
    CHECK(bb.write_line(LINE, LINE_LEN));
    CHECK(bb.ok());
    // Degenerate inputs return false without flipping ok().
    CHECK(!bb.write_line(LINE, 0));
    CHECK(bb.ok());
    cleanup();
    return 0;
}

int test_write_after_open_failure_is_false() {
    // An unopenable path (missing directory) means ok() is false from the
    // start, and write_line reports failure rather than pretending success.
    Blackbox bb("/nonexistent_dir_riposte/bb.jsonl", 0, 0);
    CHECK(!bb.ok());
    CHECK(!bb.write_line(LINE, LINE_LEN));
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_rotation_and_retention();
    rc = rc != 0 ? rc : test_append_preserves_existing();
    rc = rc != 0 ? rc : test_keep_zero_truncates();
    rc = rc != 0 ? rc : test_write_line_reports_success();
    rc = rc != 0 ? rc : test_write_after_open_failure_is_false();
    if (rc == 0) {
        std::printf("test_blackbox: %d checks passed\n", checks);
    }
    return rc;
}
