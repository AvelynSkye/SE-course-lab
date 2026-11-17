

#ifndef SERVICE_ACTIVITY_SERVICE_H_
#define SERVICE_ACTIVITY_SERVICE_H_

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

class ActivityService {
 public:
  ActivityService(infra::DataStore* db, RoleService* rls, AuditService* aud,
                  NotificationService* ntfs, TicketService* tkts);

  ServiceResult CreateActivity(domain::Id orgId, const std::string& ttl,
                               const std::string& desc, domain::Id* aid);
  ServiceResult EditDraft(domain::Id orgId, domain::Id vid,
                          const std::string& ttl, const std::string& desc);
  ServiceResult SubmitReview(domain::Id orgId, domain::Id vid);
  ServiceResult Review(domain::Id admId, domain::Id vid, bool apv,
                       const std::string& rsn);
  ServiceResult Publish(domain::Id orgId, domain::Id vid);
  ServiceResult CancelActivity(domain::Id orgId, domain::Id aid);
  ServiceResult ValidateForPublish(domain::Id vid, std::string* rsn);

  std::vector<domain::ActivityVersion> PendingVersions();
  std::vector<domain::Activity> ActivitiesOf(domain::Id orgId);
  std::vector<domain::Activity> AllActivities();
  std::vector<domain::ActivityVersion> VersionsOf(domain::Id aid);
  domain::Activity* FindActivity(domain::Id aid);
  domain::ActivityVersion* FindVersion(domain::Id vid);
  domain::ActivityVersion* CurrentVersion(domain::Id aid);
  domain::ActivityVersion* LatestVersion(domain::Id aid);
  std::string DescribeVersion(domain::Id vid);

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
