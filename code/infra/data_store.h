

#ifndef INFRA_DATA_STORE_H_
#define INFRA_DATA_STORE_H_

#include <map>
#include <string>
#include <vector>

#include "domain/entities.h"

namespace campus {
namespace infra {

class DataStore {
 public:
  DataStore();

  domain::Id NextId(const std::string& tbl);

  std::vector<domain::User>& usrs() { return usrs_; }
  std::vector<domain::UserRole>& user_roles() { return usrRls_; }
  std::vector<domain::Activity>& acts() { return acts_; }
  std::vector<domain::ActivityVersion>& activity_versions() { return actVers_; }
  std::vector<domain::LogicalSession>& logical_sessions() { return lSsns_; }
  std::vector<domain::Session>& ssns() { return ssns_; }
  std::vector<domain::LogicalCategory>& logical_categories() { return lCats_; }
  std::vector<domain::CategoryVersion>& category_versions() { return catVers_; }
  std::vector<domain::SessionGrant>& session_grants() { return ssnGrs_; }
  std::vector<domain::Registration>& regs() { return regs_; }
  std::vector<domain::Ticket>& tkts() { return tkts_; }
  std::vector<domain::CheckinEvent>& checkin_events() { return chkEvs_; }
  std::vector<domain::Notification>& ntfs() { return ntfs_; }
  std::vector<domain::AuditLog>& audit_logs() { return audLogs_; }
  std::vector<domain::IdempotencyRecord>& idempotency_records() {
    return idemRecs_;
  }
  std::vector<domain::BackgroundTask>& background_tasks() { return bgTsks_; }

  bool Load(const std::string& dir);
  bool Save(const std::string& dir) const;
  void Clear();

 private:
  void RefreshCounters();

  std::map<std::string, domain::Id> cnts_;
  std::vector<domain::User> usrs_;
  std::vector<domain::UserRole> usrRls_;
  std::vector<domain::Activity> acts_;
  std::vector<domain::ActivityVersion> actVers_;
  std::vector<domain::LogicalSession> lSsns_;
  std::vector<domain::Session> ssns_;
  std::vector<domain::LogicalCategory> lCats_;
  std::vector<domain::CategoryVersion> catVers_;
  std::vector<domain::SessionGrant> ssnGrs_;
  std::vector<domain::Registration> regs_;
  std::vector<domain::Ticket> tkts_;
  std::vector<domain::CheckinEvent> chkEvs_;
  std::vector<domain::Notification> ntfs_;
  std::vector<domain::AuditLog> audLogs_;
  std::vector<domain::IdempotencyRecord> idemRecs_;
  std::vector<domain::BackgroundTask> bgTsks_;
};

}
}

#endif
