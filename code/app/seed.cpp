

#include "app/seed.h"

#include <cstddef>
#include <string>

#include "app/app_context.h"
#include "domain/datetime.h"
#include "domain/entities.h"
#include "infra/data_store.h"
#include "infra/security.h"
#include "service/ticket_service.h"

namespace campus {
namespace app {

namespace {

domain::Id AddUser(infra::DataStore* db, const std::string& acct,
                   const std::string& nm, domain::Role rl, bool forcePwd) {
  domain::User usr;
  usr.id = db->NextId("user");
  usr.acct = acct;
  usr.nm = nm;
  usr.pwdH = infra::PasswordHasher::Hash(acct, "123456");
  usr.st = domain::UserStatus::kActive;
  usr.forcePwd = forcePwd;
  db->usrs().push_back(usr);

  domain::UserRole usrRl;
  usrRl.uid = usr.id;
  usrRl.rlCd = rl;
  db->user_roles().push_back(usrRl);
  return usr.id;
}

domain::Id AddActivity(infra::DataStore* db, const std::string& ttl,
                       domain::Id crtId, domain::ActivityStatus st,
                       domain::VersionStatus versionStatus,
                       const std::string& desc, domain::Timestamp pubAt) {
  domain::Activity act;
  act.id = db->NextId("activity");
  act.ttl = ttl;
  act.st = st;
  act.crtId = crtId;
  db->acts().push_back(act);

  domain::ActivityVersion ver;
  ver.id = db->NextId("activity_version");
  ver.aid = act.id;
  ver.verNo = 1;
  ver.st = versionStatus;
  ver.desc = desc;
  ver.pubAt = pubAt;
  db->activity_versions().push_back(ver);
  return ver.id;
}

domain::Id AddSession(infra::DataStore* db, domain::Id vid,
                      const std::string& loc, domain::Timestamp begTs,
                      domain::Timestamp endTs, domain::Timestamp regBeg,
                      domain::Timestamp regEndTs, int cap) {
  domain::LogicalSession lSsn;
  lSsn.id = db->NextId("logical_session");
  lSsn.cncl = false;
  db->logical_sessions().push_back(lSsn);

  domain::Session ssn;
  ssn.id = db->NextId("session");
  ssn.lsid = lSsn.id;
  ssn.vid = vid;
  ssn.loc = loc;
  ssn.begTs = begTs;
  ssn.endTs = endTs;
  ssn.regBeg = regBeg;
  ssn.regEndTs = regEndTs;
  ssn.cap = cap;
  db->ssns().push_back(ssn);
  return lSsn.id;
}

domain::Id AddCategory(infra::DataStore* db, domain::Id vid,
                       const std::string& nm, int cap) {
  domain::LogicalCategory lCat;
  lCat.id = db->NextId("logical_category");
  lCat.cncl = false;
  db->logical_categories().push_back(lCat);

  domain::CategoryVersion cat;
  cat.id = db->NextId("category_version");
  cat.lcid = lCat.id;
  cat.vid = vid;
  cat.nm = nm;
  cat.cap = cap;
  db->category_versions().push_back(cat);
  return lCat.id;
}

domain::Id AddGrant(infra::DataStore* db, domain::Id inspId, domain::Id lsid) {
  domain::SessionGrant grt;
  grt.uid = inspId;
  grt.lsid = lsid;
  grt.st = domain::GrantStatus::kValid;
  db->session_grants().push_back(grt);
  return lsid;
}

domain::Id AddRegistration(infra::DataStore* db, domain::Id uid,
                           domain::Id lsid, domain::Id lcid, domain::Id vid,
                           domain::RegistrationStatus st,
                           domain::Timestamp crtTs) {
  domain::Registration reg;
  reg.id = db->NextId("registration");
  reg.uid = uid;
  reg.lsid = lsid;
  reg.lcid = lcid;
  reg.vid = vid;
  reg.st = st;
  reg.wlSeq = 0;
  reg.crtTs = crtTs;
  reg.cnclTs = 0;
  db->regs().push_back(reg);
  return reg.id;
}

}

void SeedDemoData(AppContext* ctx) {
  infra::DataStore* db = ctx->db;
  if (!db->usrs().empty()) {
    return;
  }
  const domain::Timestamp ts = domain::Now();

  const domain::Id adm =
      AddUser(db, "admin", "系统管理员", domain::Role::kAdministrator, false);
  (void)adm;
  const domain::Id org =
      AddUser(db, "organizer", "王组织", domain::Role::kOrganizer, false);
  const domain::Id insp =
      AddUser(db, "inspector", "李检票", domain::Role::kInspector, false);
  const domain::Id stu1 = AddUser(db, "student1", "张同学",
                                  domain::Role::kOrdinaryParticipant, true);
  const domain::Id stu2 = AddUser(db, "student2", "李同学",
                                  domain::Role::kOrdinaryParticipant, false);

  const domain::Id ver1 =
      AddActivity(db, "校园歌手大赛", org, domain::ActivityStatus::kPublished,
                  domain::VersionStatus::kCurrentPublished, "校园歌手大赛初赛",
                  domain::AddMinutes(ts, -3 * 24 * 60));
  const domain::Id ssn1 = AddSession(
      db, ver1, "大学生活动中心报告厅", domain::AddMinutes(ts, 24 * 60),
      domain::AddMinutes(ts, 26 * 60), domain::AddMinutes(ts, -2 * 24 * 60),
      domain::AddMinutes(ts, 6 * 60), 3);
  const domain::Id cat1 = AddCategory(db, ver1, "普通票", 1);
  AddCategory(db, ver1, "VIP票", 2);

  const domain::Id ver2 =
      AddActivity(db, "春日读书会", org, domain::ActivityStatus::kNotPublished,
                  domain::VersionStatus::kDraft, "图书馆共读活动", 0);
  AddSession(db, ver2, "图书馆 3 楼共享空间",
             domain::AddMinutes(ts, 3 * 24 * 60),
             domain::AddMinutes(ts, 3 * 24 * 60 + 120),
             domain::AddMinutes(ts, -24 * 60),
             domain::AddMinutes(ts, 2 * 24 * 60), 20);
  AddCategory(db, ver2, "免费票", 20);

  const domain::Id ver3 =
      AddActivity(db, "毕业晚会", org, domain::ActivityStatus::kNotPublished,
                  domain::VersionStatus::kPendingReview, "毕业生欢送晚会", 0);
  AddSession(db, ver3, "体育馆", domain::AddMinutes(ts, 5 * 24 * 60),
             domain::AddMinutes(ts, 5 * 24 * 60 + 150),
             domain::AddMinutes(ts, -24 * 60),
             domain::AddMinutes(ts, 4 * 24 * 60), 30);
  AddCategory(db, ver3, "入场券", 30);

  const domain::Id ver4 =
      AddActivity(db, "网络安全讲座", org, domain::ActivityStatus::kPublished,
                  domain::VersionStatus::kCurrentPublished, "校园网络安全专题",
                  domain::AddMinutes(ts, -5 * 24 * 60));
  const domain::Id ssn4 = AddSession(
      db, ver4, "教学楼 A101", domain::AddMinutes(ts, -24 * 60 - 120),
      domain::AddMinutes(ts, -24 * 60), domain::AddMinutes(ts, -5 * 24 * 60),
      domain::AddMinutes(ts, -2 * 24 * 60), 50);
  const domain::Id cat4 = AddCategory(db, ver4, "讲座票", 50);

  const domain::Id ver5 =
      AddActivity(db, "社团招新宣讲", org, domain::ActivityStatus::kPublished,
                  domain::VersionStatus::kCurrentPublished, "社团联合招新",
                  domain::AddMinutes(ts, -6 * 24 * 60));
  const domain::Id ssn5 = AddSession(
      db, ver5, "学生活动中心 多功能厅", domain::AddMinutes(ts, -60),
      domain::AddMinutes(ts, 180), domain::AddMinutes(ts, -5 * 24 * 60),
      domain::AddMinutes(ts, -2 * 60), 20);
  const domain::Id cat5 = AddCategory(db, ver5, "入场券", 20);

  AddGrant(db, insp, ssn1);
  AddGrant(db, insp, ssn4);
  AddGrant(db, insp, ssn5);

  const domain::Id reg1 = AddRegistration(
      db, stu2, ssn1, cat1, ver1, domain::RegistrationStatus::kRegistered,
      domain::AddMinutes(ts, -24 * 60));
  std::string tktCd;
  ctx->tkts->Issue(reg1, &tktCd);

  const domain::Id reg2 = AddRegistration(
      db, stu1, ssn4, cat4, ver4, domain::RegistrationStatus::kRegistered,
      domain::AddMinutes(ts, -4 * 24 * 60));
  ctx->tkts->Issue(reg2, &tktCd);

  const domain::Id reg3 = AddRegistration(
      db, stu2, ssn5, cat5, ver5, domain::RegistrationStatus::kRegistered,
      domain::AddMinutes(ts, -3 * 24 * 60));
  ctx->tkts->Issue(reg3, &tktCd);

  domain::BackgroundTask tsk;
  tsk.id = db->NextId("background_task");
  tsk.tskTyp = "notification_retry";
  tsk.pl = "seed-notification";
  tsk.st = domain::TaskStatus::kFailed;
  tsk.retryN = 0;
  db->background_tasks().push_back(tsk);
}

}
}
