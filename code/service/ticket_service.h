

#ifndef SERVICE_TICKET_SERVICE_H_
#define SERVICE_TICKET_SERVICE_H_

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

class TicketService {
 public:
  TicketService(infra::DataStore* db, RoleService* rls, AuditService* aud);

  ServiceResult Issue(domain::Id rid, std::string* cd);
  ServiceResult Void(domain::Id rid);
  ServiceResult CheckIn(domain::Id inspId, const std::string& cd);

  domain::Ticket* FindByCode(const std::string& cd);
  domain::Ticket* FindByRegistration(domain::Id rid);
  domain::Registration* FindRegistration(domain::Id rid);
  std::string Describe(domain::Id rid);
  bool InCheckinWindow(const domain::Session& ssn, domain::Timestamp ts);

 private:
  infra::DataStore* db_;
  RoleService* rls_;
  AuditService* aud_;
};

}
}

#endif
