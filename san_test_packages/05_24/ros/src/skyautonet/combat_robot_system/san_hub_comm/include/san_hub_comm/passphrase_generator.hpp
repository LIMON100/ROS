// SAN v1.3 PHASE 5 - SRT AES-128 passphrase generator.
//
// SRT's `passphrase` URI parameter accepts 10-79 ASCII bytes; the
// actual AES-128 key is derived internally. We pick 32 random
// alphanumeric chars per stream (≈190 bits of entropy), regenerated
// on every START transition so a captured operator session can't
// be replayed.
//
// Source of randomness: std::random_device (hardware-backed where
// available). For real deployments where you want guaranteed
// cryptographic strength, swap in OpenSSL RAND_bytes() - the
// interface is the same.

#pragma once

#include <cstddef>
#include <string>

namespace san_hub_comm {

class PassphraseGenerator {
public:
    static constexpr std::size_t kDefaultLen = 32;
    static constexpr std::size_t kMinLen = 10;     // SRT lower bound
    static constexpr std::size_t kMaxLen = 79;     // SRT upper bound

    explicit PassphraseGenerator(std::size_t len = kDefaultLen);

    // Generate a fresh passphrase. Returns a kLen-character string of
    // alphanumeric characters [A-Za-z0-9].
    std::string generate();

    std::size_t length() const { return len_; }

private:
    std::size_t len_;
};

}  // namespace san_hub_comm
