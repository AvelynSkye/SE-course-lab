

#ifndef SERVICE_SESSION_SERVICE_H_
#define SERVICE_SESSION_SERVICE_H_

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
class NotificationService;
class RoleService;
class TicketService;

class SessionService {
 public:
  SessionService(infra::DataStore* db, RoleService* rls, AuditService* aud,
                 NotificationService* ntfs, TicketService* tkts);

  ServiceResult CreateSession(domain::Id orgId, domain::Id vid,
                              const std::string& loc, domain::Timestamp begTs,
                              domain::Timestamp endTs, domain::Timestamp regBeg,
                              domain::Timestamp regEndTs, int cap,
                              domain::Id* lsid);
  ServiceResult CreateCategory(domain::Id orgId, domain::Id vid,
                               const std::string& nm, int cap,
                               domain::Id* lcid);
  ServiceResult UpdateCategoryQuota(domain::Id orgId, domain::Id cvid, int cap);
  ServiceResult CancelSession(domain::Id orgId, domain::Id lsid);
  ServiceResult CancelCategory(domain::Id orgId, domain::Id lcid);

  std::vector<domain::Session> SessionsOfVersion(domain::Id vid);
  std::vector<domain::CategoryVersion> CategoriesOfVersion(domain::Id vid);
  std::vector<domain::LogicalSession> AllLogicalSessions();
  domain::Session* FindSession(domain::Id lsid, domain::Id vid);
  domain::CategoryVersion* FindCategory(domain::Id lcid, domain::Id vid);
  domain::LogicalSession* FindLogicalSession(domain::Id lsid);
  domain::LogicalCategory* FindLogicalCategory(domain::Id lcid);
  bool VersionEditable(domain::Id orgId, domain::Id vid, std::string* rsn);

 private:
  infra::DataStore* db_;
  RoleService* rls_;
  AuditService* aud_;
  NotificationService* ntfs_;
  TicketService* tkts_;
};

}
}

#endif
