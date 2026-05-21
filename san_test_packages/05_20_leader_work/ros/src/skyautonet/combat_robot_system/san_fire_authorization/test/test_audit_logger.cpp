// SAN v1.5 PHASE 9 — AuditLogger unit tests.
//
// Coverage:
//   A1  emit() returns non-empty uuid + self_hash on success
//   A2  UUIDv4 format (RFC 4122): 8-4-4-4-12 hex, version=4, variant=8/9/a/b
//   A3  Each emit produces a unique uuid
//   A4  Successive entries form a valid hash chain (prev_hash → self_hash)
//   A5  Genesis: first entry's prev_hash == kGenesisHash
//   A6  File mode is 0640 after construction
//   A7  JSON Lines: each line is parse-able + ends with \n
//   A8  limp_mode_fire field round-trips through the log
//   A9  verifyChain() PASS on a freshly-written file
//   A10 verifyChain() FAIL when middle line is tampered (self_hash)
//   A11 verifyChain() FAIL when prev_hash chain is broken
//   A12 Re-open existing file — prev_hash continues from tail
//   A13 sha256Hex matches known RFC 6234 test vector
//   A14 Concurrent emit from multiple threads — all entries written, chain valid

#include "san_fire_authorization/audit_logger.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

namespace san_fire_authorization {
namespace {

// ─── helpers ────────────────────────────────────────────────────────────

std::string tempPath(const char* tag) {
  char tmpl[64];
  std::snprintf(tmpl, sizeof(tmpl), "/tmp/san_audit_%s_XXXXXX", tag);
  int fd = ::mkstemp(tmpl);
  if (fd < 0) return std::string();
  ::close(fd);
  ::unlink(tmpl);
  return std::string(tmpl);
}

AuditEntry sampleEntry(uint32_t req_id = 1, bool limp = false) {
  AuditEntry e;
  e.timestamp_ms     = 1'700'000'000'000ULL + req_id;
  e.request_id       = req_id;
  e.operator_id      = "op_taegeun";
  e.granted          = true;
  e.reason           = "GRANTED";
  e.reason_detail    = "two-key + hmac + nonce + drift all OK";
  e.limp_mode_fire   = limp;
  e.target_lat_e7    = 374200000;
  e.target_lon_e7    = 1270000000;
  e.target_alt_mm    = 250;
  e.hub_term         = 2;
  e.leader_term      = 5;
  e.n_alive_robots   = 8;
  return e;
}

std::vector<std::string> readLines(const std::string& path) {
  std::ifstream in(path);
  std::vector<std::string> out;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) out.push_back(line);
  }
  return out;
}

// ─── tests ──────────────────────────────────────────────────────────────

TEST(AuditLoggerTest, A1_EmitReturnsUuidAndSelfHash) {
  const auto path = tempPath("A1");
  ASSERT_FALSE(path.empty());

  AuditLogger lg(path);
  const auto res = lg.emit(sampleEntry());

  EXPECT_TRUE(res.ok);
  EXPECT_FALSE(res.uuid.empty());
  EXPECT_FALSE(res.self_hash.empty());
  EXPECT_EQ(res.self_hash.size(), 64u);
  EXPECT_EQ(lg.droppedCount(), 0u);

  std::remove(path.c_str());
}

TEST(AuditLoggerTest, A2_UuidV4FormatRfc4122) {
  // Generate 100 UUIDs and verify all match the RFC 4122 v4 format.
  const std::regex v4(
      "^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$");
  for (int i = 0; i < 100; ++i) {
    const auto u = AuditLogger::generateUuidV4();
    EXPECT_EQ(u.size(), 36u);
    EXPECT_TRUE(std::regex_match(u, v4))
        << "UUID does not match RFC 4122 v4: " << u;
  }
}

TEST(AuditLoggerTest, A3_EachEmitProducesUniqueUuid) {
  const auto path = tempPath("A3");
  AuditLogger lg(path);

  std::set<std::string> seen;
  constexpr int N = 50;
  for (int i = 0; i < N; ++i) {
    const auto r = lg.emit(sampleEntry(static_cast<uint32_t>(i + 1)));
    ASSERT_TRUE(r.ok);
    EXPECT_TRUE(seen.insert(r.uuid).second)
        << "Duplicate UUID at i=" << i << ": " << r.uuid;
  }
  EXPECT_EQ(seen.size(), static_cast<std::size_t>(N));

  std::remove(path.c_str());
}

TEST(AuditLoggerTest, A4_SuccessiveEntriesFormValidHashChain) {
  const auto path = tempPath("A4");
  AuditLogger lg(path);

  std::vector<std::string> self_hashes;
  for (int i = 0; i < 5; ++i) {
    const auto r = lg.emit(sampleEntry(static_cast<uint32_t>(i + 1)));
    ASSERT_TRUE(r.ok);
    self_hashes.push_back(r.self_hash);
  }

  // Read back lines + verify each prev_hash chains to the previous self_hash.
  const auto lines = readLines(path);
  ASSERT_EQ(lines.size(), 5u);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const auto& l = lines[i];
    const auto prev_pos = l.find("\"prev_hash\":\"");
    ASSERT_NE(prev_pos, std::string::npos);
    const auto prev_start = prev_pos + std::string("\"prev_hash\":\"").size();
    const std::string this_prev = l.substr(prev_start, 64);

    if (i == 0) {
      EXPECT_EQ(this_prev, kGenesisHash)
          << "First entry's prev_hash must be the genesis hash";
    } else {
      EXPECT_EQ(this_prev, self_hashes[i - 1])
          << "Entry " << i << " prev_hash must equal entry " << (i - 1)
          << "'s self_hash";
    }
  }
  std::remove(path.c_str());
}

TEST(AuditLoggerTest, A5_GenesisHashForFirstEntry) {
  const auto path = tempPath("A5");
  AuditLogger lg(path);

  EXPECT_EQ(lg.prevHash(), kGenesisHash);
  const auto r = lg.emit(sampleEntry());
  ASSERT_TRUE(r.ok);
  // After the first emit, prev_hash should advance.
  EXPECT_NE(lg.prevHash(), kGenesisHash);
  EXPECT_EQ(lg.prevHash(), r.self_hash);

  std::remove(path.c_str());
}

TEST(AuditLoggerTest, A6_FileModeIs0640) {
  const auto path = tempPath("A6");
  AuditLogger lg(path);
  // Emit at least one entry so the file definitely exists.
  ASSERT_TRUE(lg.emit(sampleEntry()).ok);

  struct stat st;
  ASSERT_EQ(::stat(path.c_str(), &st), 0);
  const auto perm = st.st_mode & 0777;
  EXPECT_EQ(perm, 0640)
      << "File mode 0640 enforced by D-004; got 0" << std::oct << perm;

  std::remove(path.c_str());
}

TEST(AuditLoggerTest, A7_EachLineEndsWithNewlineAndIsJson) {
  const auto path = tempPath("A7");
  {
    AuditLogger lg(path);
    for (int i = 0; i < 3; ++i) {
      ASSERT_TRUE(lg.emit(sampleEntry(static_cast<uint32_t>(i + 1))).ok);
    }
  }

  std::ifstream in(path);
  std::string raw((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  // Each emit() appends "\n", so total newline count == entry count.
  const auto nl = std::count(raw.begin(), raw.end(), '\n');
  EXPECT_EQ(nl, 3);
  EXPECT_FALSE(raw.empty());
  EXPECT_EQ(raw.back(), '\n');

  // Each line starts with '{' and ends with '}'.
  const auto lines = readLines(path);
  for (const auto& l : lines) {
    ASSERT_FALSE(l.empty());
    EXPECT_EQ(l.front(), '{');
    EXPECT_EQ(l.back(),  '}');
    // Verify required keys are present.
    EXPECT_NE(l.find("\"schema\":\"san.audit/v1.5\""), std::string::npos);
    EXPECT_NE(l.find("\"uuid\":\""),  std::string::npos);
    EXPECT_NE(l.find("\"prev_hash\":\""), std::string::npos);
    EXPECT_NE(l.find("\"self_hash\":\""), std::string::npos);
  }

  std::remove(path.c_str());
}

TEST(AuditLoggerTest, A8_LimpModeFireFieldRoundTrips) {
  const auto path = tempPath("A8");
  AuditLogger lg(path);

  // One non-Limp, one Limp.
  ASSERT_TRUE(lg.emit(sampleEntry(1, /*limp=*/false)).ok);
  ASSERT_TRUE(lg.emit(sampleEntry(2, /*limp=*/true)).ok);

  const auto lines = readLines(path);
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_NE(lines[0].find("\"limp_mode_fire\":false"), std::string::npos);
  EXPECT_NE(lines[1].find("\"limp_mode_fire\":true"),  std::string::npos);

  std::remove(path.c_str());
}

TEST(AuditLoggerTest, A9_VerifyChainPassesOnFreshFile) {
  const auto path = tempPath("A9");
  {
    AuditLogger lg(path);
    for (int i = 0; i < 10; ++i) {
      ASSERT_TRUE(lg.emit(sampleEntry(static_cast<uint32_t>(i + 1))).ok);
    }
  }

  const auto r = AuditLogger::verifyChain(path, kGenesisHash);
  EXPECT_TRUE(r.valid) << "verify failed: " << r.error;
  EXPECT_EQ(r.last_line_no, 9u);
  EXPECT_EQ(r.tail_self_hash.size(), 64u);

  std::remove(path.c_str());
}

TEST(AuditLoggerTest, A10_VerifyChainFailsOnTamperedSelfHash) {
  const auto path = tempPath("A10");
  {
    AuditLogger lg(path);
    for (int i = 0; i < 5; ++i) {
      ASSERT_TRUE(lg.emit(sampleEntry(static_cast<uint32_t>(i + 1))).ok);
    }
  }

  // Read all, tamper middle line's self_hash, write back.
  auto lines = readLines(path);
  ASSERT_EQ(lines.size(), 5u);
  auto& bad = lines[2];
  const auto pos = bad.find("\"self_hash\":\"");
  ASSERT_NE(pos, std::string::npos);
  // Flip one hex char inside the self_hash value.
  const auto val_start = pos + std::string("\"self_hash\":\"").size();
  bad[val_start + 5] = (bad[val_start + 5] == 'a') ? 'b' : 'a';

  std::ofstream out(path, std::ios::trunc);
  for (const auto& l : lines) out << l << "\n";
  out.close();

  const auto r = AuditLogger::verifyChain(path, kGenesisHash);
  EXPECT_FALSE(r.valid);
  // Tampered line 2 → the NEXT line (line 3) fails the chain check
  // because line 2's *recorded* self_hash no longer matches the
  // recomputed one, AND line 3's prev_hash links to the OLD self_hash.
  EXPECT_NE(r.error.find("hash"), std::string::npos)
      << "Expected hash-related failure message, got: " << r.error;

  std::remove(path.c_str());
}

TEST(AuditLoggerTest, A11_VerifyChainFailsOnBrokenPrevHash) {
  const auto path = tempPath("A11");
  {
    AuditLogger lg(path);
    for (int i = 0; i < 3; ++i) {
      ASSERT_TRUE(lg.emit(sampleEntry(static_cast<uint32_t>(i + 1))).ok);
    }
  }

  // Pass the WRONG expected_prev_hash for line 0.
  std::string wrong_genesis = kGenesisHash;
  wrong_genesis[0] = 'f';  // change first char
  const auto r = AuditLogger::verifyChain(path, wrong_genesis);
  EXPECT_FALSE(r.valid);
  EXPECT_NE(r.error.find("prev_hash"), std::string::npos)
      << "Expected prev_hash mismatch, got: " << r.error;

  std::remove(path.c_str());
}

TEST(AuditLoggerTest, A12_ReopenContinuesChainFromTail) {
  const auto path = tempPath("A12");

  std::string tail_hash;
  {
    AuditLogger lg(path);
    for (int i = 0; i < 3; ++i) {
      const auto r = lg.emit(sampleEntry(static_cast<uint32_t>(i + 1)));
      ASSERT_TRUE(r.ok);
      tail_hash = r.self_hash;
    }
  }

  // Re-open — should recover the tail hash.
  AuditLogger lg2(path);
  EXPECT_EQ(lg2.prevHash(), tail_hash)
      << "Re-opened logger must continue from the last self_hash";

  // Emit one more; verify chain still validates end-to-end.
  ASSERT_TRUE(lg2.emit(sampleEntry(4)).ok);

  const auto r = AuditLogger::verifyChain(path, kGenesisHash);
  EXPECT_TRUE(r.valid) << r.error;
  EXPECT_EQ(r.last_line_no, 3u);

  std::remove(path.c_str());
}

TEST(AuditLoggerTest, A13_Sha256HexKnownAnswer) {
  // RFC 6234 §A.1: SHA-256("abc") =
  //   ba7816bf 8f01cfea 414140de 5dae2223 b00361a3 96177a9c b410ff61 f20015ad
  const std::string got = AuditLogger::sha256Hex("abc");
  EXPECT_EQ(got,
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad");

  // Empty string:
  // e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
  EXPECT_EQ(AuditLogger::sha256Hex(""),
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855");
}

TEST(AuditLoggerTest, A14_ConcurrentEmitFromMultipleThreads) {
  const auto path = tempPath("A14");
  constexpr int kThreads = 4;
  constexpr int kPerThread = 25;
  {
    AuditLogger lg(path);
    std::atomic<int> ok_count{0};
    std::vector<std::thread> workers;
    for (int t = 0; t < kThreads; ++t) {
      workers.emplace_back([&lg, &ok_count, t]() {
        for (int i = 0; i < kPerThread; ++i) {
          const auto r = lg.emit(
              sampleEntry(static_cast<uint32_t>(t * 1000 + i)));
          if (r.ok) ok_count.fetch_add(1);
        }
      });
    }
    for (auto& w : workers) w.join();
    EXPECT_EQ(ok_count.load(), kThreads * kPerThread);
  }

  // All entries written; chain valid end-to-end.
  const auto lines = readLines(path);
  EXPECT_EQ(lines.size(), static_cast<std::size_t>(kThreads * kPerThread));

  const auto r = AuditLogger::verifyChain(path, kGenesisHash);
  EXPECT_TRUE(r.valid) << "Concurrent chain corruption: " << r.error;

  std::remove(path.c_str());
}

}  // namespace
}  // namespace san_fire_authorization
