

#ifndef DOMAIN_ENTITIES_H_
#define DOMAIN_ENTITIES_H_

#include <string>

#include "domain/enums.h"
#include "domain/types.h"

namespace campus {
namespace domain {

struct User {
  Id id = 0;
  std::string acct;
  std::string nm;
  std::string pwdH;
  UserStatus st = UserStatus::kActive;
  bool forcePwd = false;
  int failN = 0;
  Timestamp lockTs = 0;
};

struct UserRole {
  Id uid = 0;
  Role rlCd = Role::kOrdinaryParticipant;
};

struct Activity {
  Id id = 0;
  std::string ttl;
  ActivityStatus st = ActivityStatus::kNotPublished;
  Id crtId = 0;
};

struct ActivityVersion {
  Id id = 0;
  Id aid = 0;
  int verNo = 1;
  VersionStatus st = VersionStatus::kDraft;
  Timestamp pubAt = 0;
  std::string desc;
  std::string rej;
};

struct LogicalSession {
  Id id = 0;
  bool cncl = false;
};

struct Session {
  Id id = 0;
  Id lsid = 0;
  Id vid = 0;
  std::string loc;
  Timestamp begTs = 0;
  Timestamp endTs = 0;
  Timestamp regBeg = 0;
  Timestamp regEndTs = 0;
  int cap = 0;
};

struct LogicalCategory {
  Id id = 0;
  bool cncl = false;
};

struct CategoryVersion {
  Id id = 0;
  Id lcid = 0;
  Id vid = 0;
  std::string nm;
  int cap = 0;
};

struct SessionGrant {
  Id uid = 0;
  Id lsid = 0;
  GrantStatus st = GrantStatus::kValid;
};

struct Registration {
  Id id = 0;
  Id uid = 0;
  Id lsid = 0;
  Id lcid = 0;
  Id vid = 0;
  RegistrationStatus st = RegistrationStatus::kWaitlisted;
  Id wlSeq = 0;
  Timestamp crtTs = 0;
  Timestamp cnclTs = 0;
};

struct Ticket {
  Id id = 0;
  Id rid = 0;
  std::string cd;
  TicketStatus st = TicketStatus::kValid;
  Timestamp issTs = 0;
};

struct CheckinEvent {
  Id id = 0;
  Id rid = 0;
  Timestamp chkTs = 0;
};

struct Notification {
  Id id = 0;
  Id uid = 0;
  NotificationType typ = NotificationType::kPromoted;
  std::string cnt;
  bool rd = false;
  Timestamp crtTs = 0;
};

struct AuditLog {
  Id id = 0;
  Id uid = 0;
  std::string actn;
  AuditResult res = AuditResult::kSuccess;
  std::string det;
  Timestamp crtTs = 0;
};

struct IdempotencyRecord {
  std::string ky;
  std::string op;
  std::string reqH;
  std::string res;
  Timestamp crtTs = 0;
};

struct BackgroundTask {
  Id id = 0;
  std::string tskTyp;
  std::string pl;
  TaskStatus st = TaskStatus::kPending;
  int retryN = 0;
};

}
}

#endif
