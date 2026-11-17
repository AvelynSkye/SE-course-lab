

#ifndef SERVICE_AUDIT_SERVICE_H_
#define SERVICE_AUDIT_SERVICE_H_

#include <string>
#include <vector>

#include "domain/entities.h"
#include "domain/types.h"

namespace campus {
namespace infra {
class DataStore;
}

namespace service {

class AuditService {
 public:
  explicit AuditService(infra::DataStore* db);

  void Log(domain::Id uid, const std::string& actn, domain::AuditResult res,
           const std::string& det);
  std::vector<domain::AuditLog> Recent(int lim);

 private:
  infra::DataStore* db_;
};

}
}

#endif
