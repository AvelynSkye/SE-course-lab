

#ifndef APP_APP_CONTEXT_H_
#define APP_APP_CONTEXT_H_

#include <string>

namespace campus {
namespace infra {
class DataStore;
}

namespace service {
class ActivityService;
class AuditService;
class AuthService;
class BackgroundTaskService;
class NotificationService;
class RegistrationService;
class RoleService;
class SessionService;
class StatsService;
class TicketService;
}

namespace app {

struct AppContext {
  std::string ddir = "data";
  infra::DataStore* db = nullptr;
  service::AuditService* aud = nullptr;
  service::NotificationService* ntfs = nullptr;
  service::RoleService* rls = nullptr;
  service::AuthService* auth = nullptr;
  service::TicketService* tkts = nullptr;
  service::SessionService* ssns = nullptr;
  service::ActivityService* acts = nullptr;
  service::RegistrationService* regs = nullptr;
  service::StatsService* sts = nullptr;
  service::BackgroundTaskService* bg = nullptr;
};

}
}

#endif
