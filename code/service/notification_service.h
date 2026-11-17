

#ifndef SERVICE_NOTIFICATION_SERVICE_H_
#define SERVICE_NOTIFICATION_SERVICE_H_

#include <string>
#include <vector>

#include "domain/entities.h"
#include "domain/types.h"

namespace campus {
namespace infra {
class DataStore;
}

namespace service {

class NotificationService {
 public:
  explicit NotificationService(infra::DataStore* db);

  void Notify(domain::Id uid, domain::NotificationType typ,
              const std::string& cnt);
  std::vector<domain::Notification> Unread(domain::Id uid);
  int UnreadCount(domain::Id uid);
  bool MarkRead(domain::Id uid, domain::Id nid);
  int MarkAllRead(domain::Id uid);

 private:
  infra::DataStore* db_;
};

}
}

#endif
