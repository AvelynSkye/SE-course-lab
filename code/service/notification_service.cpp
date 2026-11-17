

#include "service/notification_service.h"

#include <cstddef>
#include <string>
#include <vector>

#include "domain/datetime.h"
#include "infra/data_store.h"

namespace campus {
namespace service {

NotificationService::NotificationService(infra::DataStore* db) : db_(db) {}

void NotificationService::Notify(domain::Id uid, domain::NotificationType typ,
                                 const std::string& cnt) {
  domain::Notification ntf;
  ntf.id = db_->NextId("notification");
  ntf.uid = uid;
  ntf.typ = typ;
  ntf.cnt = cnt;
  ntf.rd = false;
  ntf.crtTs = domain::Now();
  db_->ntfs().push_back(ntf);
}

std::vector<domain::Notification> NotificationService::Unread(domain::Id uid) {
  std::vector<domain::Notification> res;
  std::vector<domain::Notification>& xsAll = db_->ntfs();
  for (std::size_t i = 0; i < xsAll.size(); ++i) {
    if (xsAll[i].uid == uid && !xsAll[i].rd) {
      res.push_back(xsAll[i]);
    }
  }
  return res;
}

int NotificationService::UnreadCount(domain::Id uid) {
  int n = 0;
  std::vector<domain::Notification>& xsAll = db_->ntfs();
  for (std::size_t i = 0; i < xsAll.size(); ++i) {
    if (xsAll[i].uid == uid && !xsAll[i].rd) {
      ++n;
    }
  }
  return n;
}

bool NotificationService::MarkRead(domain::Id uid, domain::Id nid) {
  std::vector<domain::Notification>& xsAll = db_->ntfs();
  for (std::size_t i = 0; i < xsAll.size(); ++i) {
    if (xsAll[i].id == nid && xsAll[i].uid == uid) {
      xsAll[i].rd = true;
      return true;
    }
  }
  return false;
}

int NotificationService::MarkAllRead(domain::Id uid) {
  int n = 0;
  std::vector<domain::Notification>& xsAll = db_->ntfs();
  for (std::size_t i = 0; i < xsAll.size(); ++i) {
    if (xsAll[i].uid == uid && !xsAll[i].rd) {
      xsAll[i].rd = true;
      ++n;
    }
  }
  return n;
}

}
}
