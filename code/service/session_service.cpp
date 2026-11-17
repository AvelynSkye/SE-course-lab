

#include "service/session_service.h"

#include <cstddef>
#include <string>
#include <vector>

#include "domain/datetime.h"
#include "infra/data_store.h"
#include "service/audit_service.h"
#include "service/notification_service.h"
#include "service/role_service.h"
#include "service/ticket_service.h"

namespace campus {
namespace service {

namespace {

bool OwnsVersion(const std::vector<domain::ActivityVersion>& vers,
                 const std::vector<domain::Activity>& acts, domain::Id vid,
                 domain::Id orgId) {
  for (std::size_t i = 0; i < vers.size(); ++i) {
    if (vers[i].id != vid) {
      continue;
    }
    for (std::size_t j = 0; j < acts.size(); ++j) {
      if (acts[j].id == vers[i].aid) {
        return acts[j].crtId == orgId;
      }
    }
  }
  return false;
}

int ActiveRegistrationCount(const std::vector<domain::Registration>& xs,
                            domain::Id lsid, domain::Id lcid) {
  int n = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    const domain::Registration& x = xs[i];
    if (x.lsid != lsid) {
      continue;
    }
    if (lcid != 0 && x.lcid != lcid) {
      continue;
    }
    if (x.st == domain::RegistrationStatus::kRegistered ||
        x.st == domain::RegistrationStatus::kCheckedIn) {
      ++n;
    }
  }
  return n;
}

}

SessionService::SessionService(infra::DataStore* db, RoleService* rls,
                               AuditService* aud, NotificationService* ntfs,
                               TicketService* tkts)
    : db_(db), rls_(rls), aud_(aud), ntfs_(ntfs), tkts_(tkts) {}

bool SessionService::VersionEditable(domain::Id orgId, domain::Id vid,
                                     std::string* rsn) {
  domain::ActivityVersion* ver = nullptr;
  std::vector<domain::ActivityVersion>& vers = db_->activity_versions();
  for (std::size_t i = 0; i < vers.size(); ++i) {
    if (vers[i].id == vid) {
      ver = &vers[i];
      break;
    }
  }
  if (ver == nullptr) {
    *rsn = "活动版本不存在";
    return false;
  }
  if (ver->st != domain::VersionStatus::kDraft &&
      ver->st != domain::VersionStatus::kReviewRejected) {
    *rsn = "当前版本状态为" + domain::ToString(ver->st) + "，不允许编辑";
    return false;
  }
  std::vector<domain::Activity>& acts = db_->acts();
  for (std::size_t i = 0; i < acts.size(); ++i) {
    if (acts[i].id == ver->aid) {
      if (acts[i].crtId != orgId) {
        *rsn = "只能维护自己创建的活动";
        return false;
      }
      return true;
    }
  }
  *rsn = "活动不存在";
  return false;
}

ServiceResult SessionService::CreateSession(
    domain::Id orgId, domain::Id vid, const std::string& loc,
    domain::Timestamp begTs, domain::Timestamp endTs, domain::Timestamp regBeg,
    domain::Timestamp regEndTs, int cap, domain::Id* lsid) {
  if (!rls_->HasRole(orgId, domain::Role::kOrganizer)) {
    return ServiceResult::Fail("仅组织者可以维护场次");
  }
  std::string rsn;
  if (!VersionEditable(orgId, vid, &rsn)) {
    return ServiceResult::Fail(rsn);
  }
  if (loc.empty()) {
    return ServiceResult::Fail("场次地点不能为空");
  }
  if (begTs >= endTs) {
    return ServiceResult::Fail("场次结束时间必须晚于开始时间");
  }
  if (regBeg >= regEndTs) {
    return ServiceResult::Fail("报名截止时间必须晚于报名开始时间");
  }
  if (regEndTs > begTs) {
    return ServiceResult::Fail("报名截止时间不能晚于场次开始时间");
  }
  if (cap <= 0) {
    return ServiceResult::Fail("场次容量必须大于 0");
  }

  domain::LogicalSession lSsn;
  lSsn.id = db_->NextId("logical_session");
  lSsn.cncl = false;
  db_->logical_sessions().push_back(lSsn);

  domain::Session ssn;
  ssn.id = db_->NextId("session");
  ssn.lsid = lSsn.id;
  ssn.vid = vid;
  ssn.loc = loc;
  ssn.begTs = begTs;
  ssn.endTs = endTs;
  ssn.regBeg = regBeg;
  ssn.regEndTs = regEndTs;
  ssn.cap = cap;
  db_->ssns().push_back(ssn);

  *lsid = lSsn.id;
  aud_->Log(orgId, "创建场次", domain::AuditResult::kSuccess,
            "版本 " + std::to_string(vid) + " 场次 " + std::to_string(lSsn.id) +
                " " + loc);
  return ServiceResult::Ok("场次创建成功，逻辑场次 ID：" +
                           std::to_string(lSsn.id));
}

ServiceResult SessionService::CreateCategory(domain::Id orgId, domain::Id vid,
                                             const std::string& nm, int cap,
                                             domain::Id* lcid) {
  if (!rls_->HasRole(orgId, domain::Role::kOrganizer)) {
    return ServiceResult::Fail("仅组织者可以维护类别");
  }
  std::string rsn;
  if (!VersionEditable(orgId, vid, &rsn)) {
    return ServiceResult::Fail(rsn);
  }
  if (nm.empty()) {
    return ServiceResult::Fail("类别名称不能为空");
  }
  if (cap <= 0) {
    return ServiceResult::Fail("类别名额必须大于 0");
  }

  domain::LogicalCategory lCat;
  lCat.id = db_->NextId("logical_category");
  lCat.cncl = false;
  db_->logical_categories().push_back(lCat);

  domain::CategoryVersion cat;
  cat.id = db_->NextId("category_version");
  cat.lcid = lCat.id;
  cat.vid = vid;
  cat.nm = nm;
  cat.cap = cap;
  db_->category_versions().push_back(cat);

  *lcid = lCat.id;
  aud_->Log(orgId, "创建类别", domain::AuditResult::kSuccess,
            "版本 " + std::to_string(vid) + " 类别 " + nm);
  return ServiceResult::Ok("类别创建成功，逻辑类别 ID：" +
                           std::to_string(lCat.id));
}

ServiceResult SessionService::UpdateCategoryQuota(domain::Id orgId,
                                                  domain::Id cvid, int cap) {
  std::vector<domain::CategoryVersion>& cats = db_->category_versions();
  domain::CategoryVersion* tgt = nullptr;
  for (std::size_t i = 0; i < cats.size(); ++i) {
    if (cats[i].id == cvid) {
      tgt = &cats[i];
      break;
    }
  }
  if (tgt == nullptr) {
    return ServiceResult::Fail("类别版本不存在");
  }
  std::string rsn;
  if (!VersionEditable(orgId, tgt->vid, &rsn)) {
    return ServiceResult::Fail(rsn);
  }
  int usedN = 0;
  const std::vector<domain::Session>& ssns = db_->ssns();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].vid != tgt->vid) {
      continue;
    }
    const int ssnUsedN =
        ActiveRegistrationCount(db_->regs(), ssns[i].lsid, tgt->lcid);
    if (ssnUsedN > usedN) {
      usedN = ssnUsedN;
    }
  }
  if (cap < usedN) {
    return ServiceResult::Fail("名额不能小于已占用人数 " +
                               std::to_string(usedN));
  }
  if (cap <= 0) {
    return ServiceResult::Fail("名额必须大于 0");
  }
  tgt->cap = cap;
  aud_->Log(orgId, "调整类别名额", domain::AuditResult::kSuccess,
            "类别 " + tgt->nm + " 名额调整为 " + std::to_string(cap));
  return ServiceResult::Ok("名额已更新");
}

ServiceResult SessionService::CancelSession(domain::Id orgId, domain::Id lsid) {
  if (!rls_->HasRole(orgId, domain::Role::kOrganizer)) {
    return ServiceResult::Fail("仅组织者可以取消场次");
  }
  domain::LogicalSession* lSsn = FindLogicalSession(lsid);
  if (lSsn == nullptr) {
    return ServiceResult::Fail("逻辑场次不存在");
  }
  bool own = false;
  std::vector<domain::Session>& ssns = db_->ssns();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].lsid == lsid && OwnsVersion(db_->activity_versions(),
                                            db_->acts(), ssns[i].vid, orgId)) {
      own = true;
      break;
    }
  }
  if (!own) {
    return ServiceResult::Fail("只能取消自己活动的场次");
  }
  lSsn->cncl = true;

  std::vector<domain::Registration>& regs = db_->regs();
  for (std::size_t i = 0; i < regs.size(); ++i) {
    domain::Registration& reg = regs[i];
    if (reg.lsid != lsid) {
      continue;
    }
    if (reg.st == domain::RegistrationStatus::kRegistered ||
        reg.st == domain::RegistrationStatus::kWaitlisted) {
      if (reg.st == domain::RegistrationStatus::kRegistered) {
        tkts_->Void(reg.id);
      }
      reg.st = domain::RegistrationStatus::kCancelled;
      reg.cnclTs = domain::Now();
      ntfs_->Notify(reg.uid, domain::NotificationType::kSessionCancelled,
                    "您报名的场次已取消，报名已自动撤销");
    }
  }
  aud_->Log(orgId, "取消场次", domain::AuditResult::kSuccess,
            "逻辑场次 " + std::to_string(lsid));
  return ServiceResult::Ok("场次已取消，相关报名已撤销并通知");
}

ServiceResult SessionService::CancelCategory(domain::Id orgId,
                                             domain::Id lcid) {
  if (!rls_->HasRole(orgId, domain::Role::kOrganizer)) {
    return ServiceResult::Fail("仅组织者可以取消类别");
  }
  domain::LogicalCategory* cat = FindLogicalCategory(lcid);
  if (cat == nullptr) {
    return ServiceResult::Fail("逻辑类别不存在");
  }
  bool own = false;
  std::vector<domain::CategoryVersion>& cats = db_->category_versions();
  for (std::size_t i = 0; i < cats.size(); ++i) {
    if (cats[i].lcid == lcid && OwnsVersion(db_->activity_versions(),
                                            db_->acts(), cats[i].vid, orgId)) {
      own = true;
      break;
    }
  }
  if (!own) {
    return ServiceResult::Fail("只能取消自己活动的类别");
  }
  cat->cncl = true;

  std::vector<domain::Registration>& regs = db_->regs();
  for (std::size_t i = 0; i < regs.size(); ++i) {
    domain::Registration& reg = regs[i];
    if (reg.lcid != lcid ||
        (reg.st != domain::RegistrationStatus::kRegistered &&
         reg.st != domain::RegistrationStatus::kWaitlisted)) {
      continue;
    }
    if (reg.st == domain::RegistrationStatus::kRegistered) {
      tkts_->Void(reg.id);
    }
    reg.st = domain::RegistrationStatus::kCancelled;
    reg.cnclTs = domain::Now();
    ntfs_->Notify(reg.uid, domain::NotificationType::kCategoryCancelled,
                  "您报名的类别已取消，报名已自动撤销");
  }
  aud_->Log(orgId, "取消类别", domain::AuditResult::kSuccess,
            "逻辑类别 " + std::to_string(lcid));
  return ServiceResult::Ok("类别已取消，相关报名已撤销并通知");
}

std::vector<domain::Session> SessionService::SessionsOfVersion(domain::Id vid) {
  std::vector<domain::Session> res;
  std::vector<domain::Session>& ssns = db_->ssns();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].vid == vid) {
      res.push_back(ssns[i]);
    }
  }
  return res;
}

std::vector<domain::CategoryVersion> SessionService::CategoriesOfVersion(
    domain::Id vid) {
  std::vector<domain::CategoryVersion> res;
  std::vector<domain::CategoryVersion>& cats = db_->category_versions();
  for (std::size_t i = 0; i < cats.size(); ++i) {
    if (cats[i].vid == vid) {
      res.push_back(cats[i]);
    }
  }
  return res;
}

std::vector<domain::LogicalSession> SessionService::AllLogicalSessions() {
  return db_->logical_sessions();
}

domain::Session* SessionService::FindSession(domain::Id lsid, domain::Id vid) {
  std::vector<domain::Session>& ssns = db_->ssns();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].lsid == lsid && ssns[i].vid == vid) {
      return &ssns[i];
    }
  }
  return nullptr;
}

domain::CategoryVersion* SessionService::FindCategory(domain::Id lcid,
                                                      domain::Id vid) {
  std::vector<domain::CategoryVersion>& cats = db_->category_versions();
  for (std::size_t i = 0; i < cats.size(); ++i) {
    if (cats[i].lcid == lcid && cats[i].vid == vid) {
      return &cats[i];
    }
  }
  return nullptr;
}

domain::LogicalSession* SessionService::FindLogicalSession(domain::Id lsid) {
  std::vector<domain::LogicalSession>& ssns = db_->logical_sessions();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].id == lsid) {
      return &ssns[i];
    }
  }
  return nullptr;
}

domain::LogicalCategory* SessionService::FindLogicalCategory(domain::Id lcid) {
  std::vector<domain::LogicalCategory>& cats = db_->logical_categories();
  for (std::size_t i = 0; i < cats.size(); ++i) {
    if (cats[i].id == lcid) {
      return &cats[i];
    }
  }
  return nullptr;
}

}
}
