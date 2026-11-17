

#include "infra/security.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

namespace campus {
namespace infra {

namespace {

const std::uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
const std::uint64_t kFnvPrime = 1099511628211ULL;
const char kHexDigits[] = "0123456789abcdef";

std::uint64_t Fnv1a(const std::string& txt) {
  std::uint64_t h = kFnvOffsetBasis;
  for (std::size_t i = 0; i < txt.size(); ++i) {
    h ^= static_cast<unsigned char>(txt[i]);
    h *= kFnvPrime;
  }
  return h;
}

std::string ToHex(std::uint64_t val, int wid) {
  std::string txt(static_cast<std::size_t>(wid), '0');
  for (int i = wid - 1; i >= 0; --i) {
    txt[static_cast<std::size_t>(i)] = kHexDigits[val & 0xFU];
    val >>= 4;
  }
  return txt;
}

std::mt19937_64& RandomEngine() {
  std::random_device dev;
  static std::mt19937_64 rng(dev());
  return rng;
}

}

std::string PasswordHasher::Hash(const std::string& slt,
                                 const std::string& pwd) {
  const std::string kPpr = "campus-ticketing-v1";
  const std::uint64_t first = Fnv1a(slt + "|" + pwd + "|" + kPpr);
  const std::uint64_t second = Fnv1a(ToHex(first, 16) + "|" + slt);
  return ToHex(first, 16) + ToHex(second, 16);
}

bool PasswordHasher::Verify(const std::string& slt, const std::string& pwd,
                            const std::string& expH) {
  return Hash(slt, pwd) == expH;
}

std::string UuidGenerator::V4() {
  std::uniform_int_distribution<std::uint64_t> dist;
  std::uint64_t hi = dist(RandomEngine());
  std::uint64_t lo = dist(RandomEngine());
  hi = (hi & 0xFFFFFFFFFFFF0FF0ULL) | 0x0000000000004000ULL;
  lo = (lo & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
  return ToHex(hi >> 32, 8) + "-" + ToHex((hi >> 16) & 0xFFFFU, 4) + "-" +
         ToHex(hi & 0xFFFFU, 4) + "-" + ToHex(lo >> 48, 4) + "-" +
         ToHex(lo & 0xFFFFFFFFFFFFULL, 12);
}

std::string TicketCodeGenerator::Generate() {
  std::uniform_int_distribution<std::uint64_t> dist;
  const std::uint64_t val = dist(RandomEngine());
  return "T-" + ToHex(val & 0xFFFFFFFFFFFFULL, 12);
}

}
}
