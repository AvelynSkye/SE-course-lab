

#ifndef SERVICE_BACKGROUND_TASK_SERVICE_H_
#define SERVICE_BACKGROUND_TASK_SERVICE_H_

#include <string>
#include <vector>

#include "domain/entities.h"
#include "domain/types.h"

namespace campus {
namespace infra {
class DataStore;
}

namespace service {

class AuditService;
class NotificationService;

class BackgroundTaskService {
 public:
  BackgroundTaskService(infra::DataStore* db, AuditService* aud,
                        NotificationService* ntfs);

  int RunDeadlineNotification(domain::Id opId);
  int RunAbsenceDetection(domain::Id opId);
  int RetryFailedTasks(domain::Id opId);
  std::vector<domain::BackgroundTask> Tasks();

 private:
  bool TaskExists(const std::string& tskTyp, const std::string& pl);
  void Enqueue(const std::string& tskTyp, const std::string& pl,
               domain::TaskStatus st);

  infra::DataStore* db_;
  AuditService* aud_;
  NotificationService* ntfs_;
};

}
}

#endif
