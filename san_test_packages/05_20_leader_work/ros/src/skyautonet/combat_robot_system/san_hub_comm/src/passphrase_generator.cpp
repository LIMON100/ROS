#include "san_hub_comm/passphrase_generator.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>

namespace san_hub_comm {

PassphraseGenerator::PassphraseGenerator(std::size_t len)
    : len_(len)
{
    if (len_ < kMinLen || len_ > kMaxLen) {
        throw std::invalid_argument(
            "passphrase length must satisfy SRT 10..79 range");
    }
}

std::string PassphraseGenerator::generate() {
    // [A-Za-z0-9] - 62 chars, slight modulus bias is negligible at
    // this size. URL-safe so the operator URI doesn't need escaping.
    static const char kCharset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789";
    static constexpr std::size_t kCharsetSize = sizeof(kCharset) - 1;

    std::random_device rd;
    std::uniform_int_distribution<std::size_t> dist(0, kCharsetSize - 1);

    std::string out;
    out.reserve(len_);
    for (std::size_t i = 0; i < len_; ++i) {
        out.push_back(kCharset[dist(rd)]);
    }
    return out;
}

}  // namespace san_hub_comm
