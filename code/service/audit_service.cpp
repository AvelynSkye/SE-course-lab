

#include "service/audit_service.h"

#include <cstddef>
#include <string>
#include <vector>

#include "domain/datetime.h"
#include "infra/data_store.h"

namespace campus {
namespace service {

AuditService::AuditService(infra::DataStore* db) : db_(db) {}

void AuditService::Log(domain::Id uid, const std::string& actn,
                       domain::AuditResult res, const std::string& det) {
  domain::AuditLog lg;
  lg.id = db_->NextId("audit_log");
  lg.uid = uid;
  lg.actn = actn;
  lg.res = res;
  lg.det = det;
  lg.crtTs = domain::Now();
  db_->audit_logs().push_back(lg);
}

std::vector<domain::AuditLog> AuditService::Recent(int lim) {
  std::vector<domain::AuditLog> res;
  const std::vector<domain::AuditLog>& lgs = db_->audit_logs();
  if (lim <= 0) {
    return res;
  }
  std::size_t pos = 0;
  if (lgs.size() > static_cast<std::size_t>(lim)) {
    pos = lgs.size() - static_cast<std::size_t>(lim);
  }
  for (std::size_t i = pos; i < lgs.size(); ++i) {
    res.push_back(lgs[i]);
  }
  return res;
}

}
}
