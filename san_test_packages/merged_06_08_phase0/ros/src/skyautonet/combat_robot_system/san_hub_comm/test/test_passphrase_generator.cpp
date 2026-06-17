// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5 - passphrase generator unit test.

#include <gtest/gtest.h>

#include <set>
#include <string>

#include "san_hub_comm/passphrase_generator.hpp"

TEST(PassphraseGenerator, DefaultLengthIsThirtyTwo) {
  san_hub_comm::PassphraseGenerator gen;
  EXPECT_EQ(gen.length(), 32u);
  EXPECT_EQ(gen.generate().size(), 32u);
}

TEST(PassphraseGenerator, RejectsLengthsOutsideSrtRange) {
  EXPECT_THROW(
    san_hub_comm::PassphraseGenerator(9),
    std::invalid_argument);
  EXPECT_THROW(
    san_hub_comm::PassphraseGenerator(80),
    std::invalid_argument);
  EXPECT_NO_THROW(san_hub_comm::PassphraseGenerator(10));
  EXPECT_NO_THROW(san_hub_comm::PassphraseGenerator(79));
}

TEST(PassphraseGenerator, ContainsOnlyAlphanumeric) {
  san_hub_comm::PassphraseGenerator gen;
  const std::string s = gen.generate();
  for (char c : s) {
    const bool ok = (c >= 'A' && c <= 'Z') ||
      (c >= 'a' && c <= 'z') ||
      (c >= '0' && c <= '9');
    EXPECT_TRUE(ok) << "non-alphanumeric character in passphrase: " << c;
  }
}

TEST(PassphraseGenerator, ConsecutiveCallsDiffer) {
  san_hub_comm::PassphraseGenerator gen;
  std::set<std::string> seen;
  for (int i = 0; i < 100; ++i) {
    seen.insert(gen.generate());
  }
  // 32 chars * log2(62) ~ 190 bits; collisions in 100 samples are
  // astronomically unlikely.
  EXPECT_EQ(seen.size(), 100u);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
