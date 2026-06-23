// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-014 Item 8 gtest.

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "san_bringup/network_bringup.hpp"

using san_bringup::network::lookupIp;
using san_bringup::network::parseInt;
using san_bringup::network::trim;

TEST(LookupIp, AllSwarmMembersMapped) {
  EXPECT_STREQ(lookupIp(1, 0), "192.168.50.10");
  EXPECT_STREQ(lookupIp(2, 1), "192.168.50.20");
  EXPECT_STREQ(lookupIp(2, 2), "192.168.50.21");
  EXPECT_STREQ(lookupIp(3, 0), "192.168.50.30");
  EXPECT_STREQ(lookupIp(4, 0), "192.168.50.40");
  EXPECT_STREQ(lookupIp(5, 0), "192.168.50.41");
  EXPECT_STREQ(lookupIp(6, 0), "192.168.50.42");
  EXPECT_STREQ(lookupIp(7, 0), "192.168.50.43");
  EXPECT_STREQ(lookupIp(8, 0), "192.168.50.44");
}

TEST(LookupIp, UnknownCombinationsReturnNullptr) {
  EXPECT_EQ(lookupIp(0, 0), nullptr);
  EXPECT_EQ(lookupIp(1, 1), nullptr);  // Leader is single-SBC
  EXPECT_EQ(lookupIp(2, 0), nullptr);  // Hub requires sbc_id 1 or 2
  EXPECT_EQ(lookupIp(2, 3), nullptr);
  EXPECT_EQ(lookupIp(9, 0), nullptr);  // out of range high
  EXPECT_EQ(lookupIp(-1, 0), nullptr);  // out of range low
}

TEST(LookupIp, IpFormatIsCorrect) {
  // Sanity: every returned IP is a 4-octet IPv4 literal with the
  // expected prefix. Cheap regression guard against accidentally
  // typing "192.168.5.10" or similar.
  for (int rid = 1; rid <= 8; ++rid) {
    for (int sid = 0; sid <= 2; ++sid) {
      const char * ip = lookupIp(rid, sid);
      if (!ip) {continue;}
      std::string s(ip);
      EXPECT_EQ(s.rfind("192.168.50.", 0), 0u)
        << "robot " << rid << "/" << sid << " IP " << s
        << " missing 192.168.50.* prefix";
    }
  }
}

TEST(ParseInt, AcceptsValidIntegers) {
  EXPECT_EQ(parseInt("0"), 0);
  EXPECT_EQ(parseInt("42"), 42);
  EXPECT_EQ(parseInt("-5"), -5);
  EXPECT_EQ(parseInt("+7"), 7);
  EXPECT_EQ(parseInt("  3  "), 3);      // trims whitespace
  EXPECT_EQ(parseInt("\n9\n"), 9);
}

TEST(ParseInt, RejectsGarbage) {
  EXPECT_EQ(parseInt(""), std::nullopt);
  EXPECT_EQ(parseInt("abc"), std::nullopt);
  EXPECT_EQ(parseInt("12.3"), std::nullopt);
  EXPECT_EQ(parseInt("1a"), std::nullopt);
  EXPECT_EQ(parseInt("--3"), std::nullopt);
  EXPECT_EQ(parseInt("+"), std::nullopt);
}

TEST(Trim, BasicCases) {
  EXPECT_EQ(trim(""), "");
  EXPECT_EQ(trim("hello"), "hello");
  EXPECT_EQ(trim("  hello  "), "hello");
  EXPECT_EQ(trim("\nhello\n"), "hello");
  EXPECT_EQ(trim("\t  hi \r\n"), "hi");
}
