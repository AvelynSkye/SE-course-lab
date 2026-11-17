

#ifndef SERVICE_ROLE_SERVICE_H_
#define SERVICE_ROLE_SERVICE_H_

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

class RoleService {
 public:
  RoleService(infra::DataStore* db, AuditService* aud);

  std::vector<domain::Role> RolesOf(domain::Id uid);
  bool HasRole(domain::Id uid, domain::Role rl);
  bool CanAccess(domain::Role rl, const std::string& modCd);

  ServiceResult AssignRole(domain::Id opId, domain::Id tgtUid, domain::Role rl);
  ServiceResult RemoveRole(domain::Id opId, domain::Id tgtUid, domain::Role rl);
  ServiceResult GrantSession(domain::Id opId, domain::Id inspId,
                             domain::Id lsid);
  std::vector<domain::SessionGrant> GrantsOf(domain::Id uid);
  bool HasValidGrant(domain::Id inspId, domain::Id lsid);
  bool UserExists(domain::Id uid);

 private:
  infra::DataStore* db_;
  AuditService* aud_;
};

}
}

#endif
