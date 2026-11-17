

#ifndef INFRA_SECURITY_H_
#define INFRA_SECURITY_H_

#include <string>

namespace campus {
namespace infra {

class PasswordHasher {
 public:
  static std::string Hash(const std::string& slt, const std::string& pwd);
  static bool Verify(const std::string& slt, const std::string& pwd,
                     const std::string& expH);
};

class UuidGenerator {
 public:
  static std::string V4();
};

class TicketCodeGenerator {
 public:
  static std::string Generate();
};

}
}

#endif
