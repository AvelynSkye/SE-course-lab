

#include "service/activity_service.h"

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

domain::ActivityVersion* FindVersionIn(
    std::vector<domain::ActivityVersion>& vers, domain::Id vid) {
  for (std::size_t i = 0; i < vers.size(); ++i) {
    if (vers[i].id == vid) {
      return &vers[i];
    }
  }
  return nullptr;
}

domain::Activity* FindActivityIn(std::vector<domain::Activity>& acts,
                                 domain::Id aid) {
  for (std::size_t i = 0; i < acts.size(); ++i) {
    if (acts[i].id == aid) {
      return &acts[i];
    }
  }
  return nullptr;
}

std::vector<domain::Session> SessionsIn(
    const std::vector<domain::Session>& ssns, domain::Id vid) {
  std::vector<domain::Session> res;
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].vid == vid) {
      res.push_back(ssns[i]);
    }
  }
  return res;
}

std::vector<domain::CategoryVersion> CategoriesIn(
    const std::vector<domain::CategoryVersion>& cats, domain::Id vid) {
  std::vector<domain::CategoryVersion> res;
  for (std::size_t i = 0; i < cats.size(); ++i) {
    if (cats[i].vid == vid) {
      res.push_back(cats[i]);
    }
  }
  return res;
}

bool Overlaps(domain::Timestamp b1, domain::Timestamp e1, domain::Timestamp b2,
              domain::Timestamp e2) {
  return b1 < e2 && b2 < e1;
}

bool ContainsId(const std::vector<domain::Id>& ids, domain::Id val) {
  for (std::size_t i = 0; i < ids.size(); ++i) {
    if (ids[i] == val) {
      return true;
    }
  }
  return false;
}

bool LogicalSessionCancelled(const std::vector<domain::LogicalSession>& ssns,
                             domain::Id lsid) {
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].id == lsid) {
      return ssns[i].cncl;
    }
  }
  return true;
}

bool LogicalCategoryCancelled(const std::vector<domain::LogicalCategory>& cats,
                              domain::Id lcid) {
  for (std::size_t i = 0; i < cats.size(); ++i) {
    if (cats[i].id == lcid) {
      return cats[i].cncl;
    }
  }
  return true;
}

}

ActivityService::ActivityService(infra::DataStore* db, RoleService* rls,
                                 AuditService* aud, NotificationService* ntfs,
                                 TicketService* tkts)
    : db_(db), rls_(rls), aud_(aud), ntfs_(ntfs), tkts_(tkts) {}

ServiceResult ActivityService::CreateActivity(domain::Id orgId,
                                              const std::string& ttl,
                                              const std::string& desc,
                                              domain::Id* aid) {
  if (!rls_->HasRole(orgId, domain::Role::kOrganizer)) {
    return ServiceResult::Fail("仅组织者可以创建活动");
  }
  if (ttl.empty()) {
    return ServiceResult::Fail("活动标题不能为空");
  }
  domain::Activity act;
  act.id = db_->NextId("activity");
  act.ttl = ttl;
  act.st = domain::ActivityStatus::kNotPublished;
  act.crtId = orgId;
  db_->acts().push_back(act);

  domain::ActivityVersion ver;
  ver.id = db_->NextId("activity_version");
  ver.aid = act.id;
  ver.verNo = 1;
  ver.st = domain::VersionStatus::kDraft;
  ver.desc = desc;
  db_->activity_versions().push_back(ver);

  *aid = act.id;
  aud_->Log(orgId, "创建活动", domain::AuditResult::kSuccess, ttl);
  return ServiceResult::Ok("活动创建成功，活动 ID：" + std::to_string(act.id) +
                           "（初始版本为草稿）");
}

ServiceResult ActivityService::EditDraft(domain::Id orgId, domain::Id vid,
                                         const std::string& ttl,
                                         const std::string& desc) {
  domain::ActivityVersion* ver = FindVersion(vid);
  if (ver == nullptr) {
    return ServiceResult::Fail("活动版本不存在");
  }
  domain::Activity* act = FindActivity(ver->aid);
  if (act == nullptr) {
    return ServiceResult::Fail("活动不存在");
  }
  if (act->crtId != orgId) {
    return ServiceResult::Fail("只能编辑自己创建的活动");
  }
  if (ver->st != domain::VersionStatus::kDraft &&
      ver->st != domain::VersionStatus::kReviewRejected) {
    return ServiceResult::Fail("当前状态不可编辑：" +
                               domain::ToString(ver->st));
  }
  if (ttl.empty()) {
    return ServiceResult::Fail("活动标题不能为空");
  }
  act->ttl = ttl;
  ver->desc = desc;
  ver->st = domain::VersionStatus::kDraft;
  ver->rej.clear();
  aud_->Log(orgId, "编辑活动草稿", domain::AuditResult::kSuccess,
            "版本 " + std::to_string(vid));
  return ServiceResult::Ok("草稿已保存");
}

ServiceResult ActivityService::ValidateForPublish(domain::Id vid,
                                                  std::string* rsn) {
  domain::ActivityVersion* ver = FindVersion(vid);
  if (ver == nullptr) {
    *rsn = "活动版本不存在";
    return ServiceResult::Fail(*rsn);
  }
  domain::Activity* act = FindActivity(ver->aid);
  if (act == nullptr) {
    *rsn = "活动不存在";
    return ServiceResult::Fail(*rsn);
  }
  if (act->ttl.empty()) {
    *rsn = "活动标题为空";
    return ServiceResult::Fail(*rsn);
  }
  const std::vector<domain::Session> ssns = SessionsIn(db_->ssns(), vid);
  const std::vector<domain::CategoryVersion> cats =
      CategoriesIn(db_->category_versions(), vid);
  if (ssns.empty()) {
    *rsn = "至少需要配置一个场次";
    return ServiceResult::Fail(*rsn);
  }
  if (cats.empty()) {
    *rsn = "至少需要配置一个类别";
    return ServiceResult::Fail(*rsn);
  }
  int minCap = 0;
  std::vector<domain::Id> sids;
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    const domain::Session& ssn = ssns[i];
    if (ContainsId(sids, ssn.lsid)) {
      *rsn = "同一版本内场次逻辑 ID 重复";
      return ServiceResult::Fail(*rsn);
    }
    sids.push_back(ssn.lsid);
    if (LogicalSessionCancelled(db_->logical_sessions(), ssn.lsid)) {
      *rsn = "存在已取消的逻辑场次";
      return ServiceResult::Fail(*rsn);
    }
    if (ssn.loc.empty()) {
      *rsn = "场次地点为空";
      return ServiceResult::Fail(*rsn);
    }
    if (ssn.begTs >= ssn.endTs) {
      *rsn = "场次时间区间非法";
      return ServiceResult::Fail(*rsn);
    }
    if (ssn.regBeg >= ssn.regEndTs) {
      *rsn = "报名时间区间非法";
      return ServiceResult::Fail(*rsn);
    }
    if (ssn.regEndTs > ssn.begTs) {
      *rsn = "报名截止晚于场次开始";
      return ServiceResult::Fail(*rsn);
    }
    if (ssn.cap <= 0) {
      *rsn = "场次容量必须大于 0";
      return ServiceResult::Fail(*rsn);
    }
    if (i == 0 || ssn.cap < minCap) {
      minCap = ssn.cap;
    }
  }
  int capSum = 0;
  std::vector<domain::Id> cids;
  for (std::size_t i = 0; i < cats.size(); ++i) {
    const domain::CategoryVersion& cat = cats[i];
    if (ContainsId(cids, cat.lcid)) {
      *rsn = "同一版本内类别逻辑 ID 重复";
      return ServiceResult::Fail(*rsn);
    }
    cids.push_back(cat.lcid);
    if (LogicalCategoryCancelled(db_->logical_categories(), cat.lcid)) {
      *rsn = "存在已取消的逻辑类别";
      return ServiceResult::Fail(*rsn);
    }
    if (cat.nm.empty()) {
      *rsn = "类别名称为空";
      return ServiceResult::Fail(*rsn);
    }
    if (cat.cap <= 0) {
      *rsn = "类别名额必须大于 0";
      return ServiceResult::Fail(*rsn);
    }
    capSum += cat.cap;
  }
  if (capSum > minCap) {
    *rsn = "类别名额合计超过场次容量";
    return ServiceResult::Fail(*rsn);
  }

  std::vector<domain::Activity>& acts = db_->acts();
  for (std::size_t i = 0; i < acts.size(); ++i) {
    if (acts[i].id == act->id ||
        acts[i].st != domain::ActivityStatus::kPublished) {
      continue;
    }
    domain::ActivityVersion* oVer = CurrentVersion(acts[i].id);
    if (oVer == nullptr) {
      continue;
    }
    const std::vector<domain::Session> oSsns =
        SessionsIn(db_->ssns(), oVer->id);
    for (std::size_t j = 0; j < ssns.size(); ++j) {
      for (std::size_t k = 0; k < oSsns.size(); ++k) {
        if (ssns[j].loc == oSsns[k].loc &&
            Overlaps(ssns[j].begTs, ssns[j].endTs, oSsns[k].begTs,
                     oSsns[k].endTs)) {
          *rsn = "与已发布活动《" + acts[i].ttl + "》存在同地点时间冲突";
          return ServiceResult::Fail(*rsn);
        }
      }
    }
  }
  *rsn = "发布前复核通过";
  return ServiceResult::Ok(*rsn);
}

ServiceResult ActivityService::SubmitReview(domain::Id orgId, domain::Id vid) {
  if (!rls_->HasRole(orgId, domain::Role::kOrganizer)) {
    return ServiceResult::Fail("仅组织者可以提交审核");
  }
  domain::ActivityVersion* ver = FindVersion(vid);
  if (ver == nullptr) {
    return ServiceResult::Fail("活动版本不存在");
  }
  domain::Activity* act = FindActivity(ver->aid);
  if (act == nullptr || act->crtId != orgId) {
    return ServiceResult::Fail("只能提交自己创建的活动");
  }
  if (ver->st != domain::VersionStatus::kDraft &&
      ver->st != domain::VersionStatus::kReviewRejected) {
    return ServiceResult::Fail("当前状态不可提交审核：" +
                               domain::ToString(ver->st));
  }
  std::string rsn;
  ServiceResult checked = ValidateForPublish(vid, &rsn);
  if (!checked.ok) {
    aud_->Log(orgId, "提交审核", domain::AuditResult::kFailure, rsn);
    return ServiceResult::Fail("内容校验未通过：" + rsn);
  }
  ver->st = domain::VersionStatus::kPendingReview;
  ver->rej.clear();
  aud_->Log(orgId, "提交审核", domain::AuditResult::kSuccess,
            "版本 " + std::to_string(vid));
  return ServiceResult::Ok("已提交审核，等待管理员处理");
}

ServiceResult ActivityService::Review(domain::Id admId, domain::Id vid,
                                      bool apv, const std::string& rsn) {
  if (!rls_->HasRole(admId, domain::Role::kAdministrator)) {
    return ServiceResult::Fail("仅管理员可以审核活动");
  }
  domain::ActivityVersion* ver = FindVersion(vid);
  if (ver == nullptr) {
    return ServiceResult::Fail("活动版本不存在");
  }
  if (ver->st != domain::VersionStatus::kPendingReview) {
    return ServiceResult::Fail("该版本不在待审核状态");
  }
  domain::Activity* act = FindActivity(ver->aid);
  if (act == nullptr) {
    return ServiceResult::Fail("活动不存在");
  }

  if (act->crtId == admId) {
    aud_->Log(admId, "审核活动", domain::AuditResult::kFailure,
              "触发审核回避规则");
    return ServiceResult::Fail("审核回避：不能审核自己创建的活动");
  }
  if (apv) {
    ver->st = domain::VersionStatus::kApprovedPendingPublish;
    aud_->Log(admId, "审核活动", domain::AuditResult::kSuccess,
              "通过版本 " + std::to_string(vid));
    return ServiceResult::Ok("审核通过，等待组织者发布");
  }
  if (rsn.empty()) {
    return ServiceResult::Fail("驳回时必须填写原因");
  }
  ver->st = domain::VersionStatus::kReviewRejected;
  ver->rej = rsn;
  aud_->Log(admId, "审核活动", domain::AuditResult::kSuccess,
            "驳回版本 " + std::to_string(vid) + "：" + rsn);
  return ServiceResult::Ok("已驳回，原因已记录");
}

ServiceResult ActivityService::Publish(domain::Id orgId, domain::Id vid) {
  domain::ActivityVersion* ver = FindVersion(vid);
  if (ver == nullptr) {
    return ServiceResult::Fail("活动版本不存在");
  }
  domain::Activity* act = FindActivity(ver->aid);
  if (act == nullptr || act->crtId != orgId) {
    return ServiceResult::Fail("只能发布自己创建的活动");
  }
  if (ver->st != domain::VersionStatus::kApprovedPendingPublish) {
    return ServiceResult::Fail("只有审核通过待发布的版本才能发布，当前：" +
                               domain::ToString(ver->st));
  }
  std::string rsn;
  ServiceResult checked = ValidateForPublish(vid, &rsn);
  if (!checked.ok) {
    aud_->Log(orgId, "发布活动", domain::AuditResult::kFailure, rsn);
    return ServiceResult::Fail("发布复核失败：" + rsn);
  }
  std::vector<domain::ActivityVersion>& vers = db_->activity_versions();
  for (std::size_t i = 0; i < vers.size(); ++i) {
    if (vers[i].aid == act->id && vers[i].id != vid &&
        vers[i].st == domain::VersionStatus::kCurrentPublished) {
      vers[i].st = domain::VersionStatus::kHistory;
    }
  }
  ver->st = domain::VersionStatus::kCurrentPublished;
  ver->pubAt = domain::Now();
  act->st = domain::ActivityStatus::kPublished;
  aud_->Log(orgId, "发布活动", domain::AuditResult::kSuccess,
            act->ttl + " 版本 " + std::to_string(vid));
  return ServiceResult::Ok("发布成功，当前发布版本已更新");
}

ServiceResult ActivityService::CancelActivity(domain::Id orgId,
                                              domain::Id aid) {
  domain::Activity* act = FindActivity(aid);
  if (act == nullptr) {
    return ServiceResult::Fail("活动不存在");
  }
  if (act->crtId != orgId) {
    return ServiceResult::Fail("只能取消自己创建的活动");
  }
  if (act->st == domain::ActivityStatus::kCancelled) {
    return ServiceResult::Fail("活动已取消");
  }
  domain::ActivityVersion* ver = CurrentVersion(aid);
  act->st = domain::ActivityStatus::kCancelled;
  if (ver == nullptr) {
    aud_->Log(orgId, "取消活动", domain::AuditResult::kSuccess, act->ttl);
    return ServiceResult::Ok("活动已取消");
  }
  ver->st = domain::VersionStatus::kHistory;
  const std::vector<domain::Session> ssns = SessionsIn(db_->ssns(), ver->id);
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    domain::LogicalSession* lSsn = nullptr;
    std::vector<domain::LogicalSession>& lSsns = db_->logical_sessions();
    for (std::size_t j = 0; j < lSsns.size(); ++j) {
      if (lSsns[j].id == ssns[i].lsid) {
        lSsn = &lSsns[j];
        break;
      }
    }
    if (lSsn != nullptr) {
      lSsn->cncl = true;
    }
  }
  std::vector<domain::Registration>& regs = db_->regs();
  for (std::size_t i = 0; i < regs.size(); ++i) {
    domain::Registration& reg = regs[i];
    if (reg.vid != ver->id) {
      continue;
    }
    if (reg.st == domain::RegistrationStatus::kRegistered ||
        reg.st == domain::RegistrationStatus::kWaitlisted) {
      if (reg.st == domain::RegistrationStatus::kRegistered) {
        tkts_->Void(reg.id);
      }
      reg.st = domain::RegistrationStatus::kCancelled;
      reg.cnclTs = domain::Now();
      ntfs_->Notify(reg.uid, domain::NotificationType::kActivityCancelled,
                    "您报名的活动《" + act->ttl + "》已取消，报名已自动撤销");
    }
  }
  aud_->Log(orgId, "取消活动", domain::AuditResult::kSuccess, act->ttl);
  return ServiceResult::Ok("活动已取消，相关报名已撤销并通知");
}

std::vector<domain::ActivityVersion> ActivityService::PendingVersions() {
  std::vector<domain::ActivityVersion> res;
  std::vector<domain::ActivityVersion>& vers = db_->activity_versions();
  for (std::size_t i = 0; i < vers.size(); ++i) {
    if (vers[i].st == domain::VersionStatus::kPendingReview) {
      res.push_back(vers[i]);
    }
  }
  return res;
}

std::vector<domain::Activity> ActivityService::ActivitiesOf(domain::Id orgId) {
  std::vector<domain::Activity> res;
  std::vector<domain::Activity>& acts = db_->acts();
  for (std::size_t i = 0; i < acts.size(); ++i) {
    if (acts[i].crtId == orgId) {
      res.push_back(acts[i]);
    }
  }
  return res;
}

std::vector<domain::Activity> ActivityService::AllActivities() {
  return db_->acts();
}

std::vector<domain::ActivityVersion> ActivityService::VersionsOf(
    domain::Id aid) {
  std::vector<domain::ActivityVersion> res;
  std::vector<domain::ActivityVersion>& vers = db_->activity_versions();
  for (std::size_t i = 0; i < vers.size(); ++i) {
    if (vers[i].aid == aid) {
      res.push_back(vers[i]);
    }
  }
  return res;
}

domain::Activity* ActivityService::FindActivity(domain::Id aid) {
  return FindActivityIn(db_->acts(), aid);
}

domain::ActivityVersion* ActivityService::FindVersion(domain::Id vid) {
  return FindVersionIn(db_->activity_versions(), vid);
}

domain::ActivityVersion* ActivityService::CurrentVersion(domain::Id aid) {
  std::vector<domain::ActivityVersion>& vers = db_->activity_versions();
  for (std::size_t i = 0; i < vers.size(); ++i) {
    if (vers[i].aid == aid &&
        vers[i].st == domain::VersionStatus::kCurrentPublished) {
      return &vers[i];
    }
  }
  return nullptr;
}

domain::ActivityVersion* ActivityService::LatestVersion(domain::Id aid) {
  domain::ActivityVersion* lat = nullptr;
  std::vector<domain::ActivityVersion>& vers = db_->activity_versions();
  for (std::size_t i = 0; i < vers.size(); ++i) {
    if (vers[i].aid != aid) {
      continue;
    }
    if (lat == nullptr || vers[i].verNo > lat->verNo) {
      lat = &vers[i];
    }
  }
  return lat;
}

std::string ActivityService::DescribeVersion(domain::Id vid) {
  domain::ActivityVersion* ver = FindVersion(vid);
  if (ver == nullptr) {
    return "版本不存在";
  }
  domain::Activity* act = FindActivity(ver->aid);
  const std::string ttl = act == nullptr ? "未知活动" : act->ttl;
  return "版本#" + std::to_string(ver->id) + " " + ttl + " V" +
         std::to_string(ver->verNo) + " [" + domain::ToString(ver->st) + "]";
}

}
}
