

#ifndef SERVICE_REGISTRATION_SERVICE_H_
#define SERVICE_REGISTRATION_SERVICE_H_

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

class RegistrationService {
 public:
  RegistrationService(infra::DataStore* db, RoleService* rls, AuditService* aud,
                      NotificationService* ntfs, TicketService* tkts);

  ServiceResult Register(domain::Id uid, domain::Id lsid, domain::Id lcid,
                         const std::string& idemKy, std::string* det);
  ServiceResult Cancel(domain::Id uid, domain::Id rid, std::string* det);

  std::vector<domain::Registration> RegistrationsOf(domain::Id uid);
  domain::Registration* Find(domain::Id rid);
  int SeatCount(domain::Id lsid, domain::Id lcid);
  int RemainingSeats(domain::Id lsid, domain::Id lcid);

 private:
  bool PromoteOne(domain::Id lsid, domain::Id lcid, std::string* det);
  std::string IdempotencyResult(const std::string& ky, const std::string& reqH,
                                bool* fnd);

  infra::DataStore* db_;
  RoleService* rls_;
  AuditService* aud_;
  NotificationService* ntfs_;
  TicketService* tkts_;
};

}
}

#endif
