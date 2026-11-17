

#ifndef SERVICE_AUTH_SERVICE_H_
#define SERVICE_AUTH_SERVICE_H_

#include <string>
#include <vector>

#include "domain/entities.h"
#include "domain/types.h"
#include "service/service_result.h"

namespace campus {
namespace infra {
class DataStore;
}

namespace service {

class AuditService;
class RoleService;

class AuthService {
 public:
  AuthService(infra::DataStore* db, AuditService* aud, RoleService* rls);

  ServiceResult Login(const std::string& acct, const std::string& pwd,
                      domain::Id* uid);
  ServiceResult ChangePassword(domain::Id uid, const std::string& oldPwd,
                               const std::string& newPwd);
  ServiceResult CreateAccount(domain::Id opId, const std::string& acct,
                              const std::string& nm, const std::string& pwd,
                              domain::Role rl, domain::Id* uid);
  ServiceResult SetStatus(domain::Id opId, domain::Id uid,
                          domain::UserStatus st);

  domain::User* FindUser(domain::Id uid);
  domain::User* FindByAccount(const std::string& acct);
  std::vector<domain::User> AllUsers();

 private:
  bool IsLocked(const domain::User& usr);

  infra::DataStore* db_;
  AuditService* aud_;
  RoleService* rls_;

  static constexpr int kMaxFailCount = 5;
  static constexpr int kLockMinutes = 15;
  static constexpr int kMinPasswordLength = 6;
};

}
}

#endif
