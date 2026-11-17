

#include "service/registration_service.h"

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

struct PublishContext {
  domain::ActivityVersion* ver = nullptr;
  domain::Activity* act = nullptr;
  domain::Session* ssn = nullptr;
  domain::CategoryVersion* cat = nullptr;
};

bool LogicalCategoryCancelled(infra::DataStore* db, domain::Id lcid) {
  const std::vector<domain::LogicalCategory>& cats = db->logical_categories();
  for (std::size_t i = 0; i < cats.size(); ++i) {
    if (cats[i].id == lcid) {
      return cats[i].cncl;
    }
  }
  return true;
}

bool ResolvePublished(infra::DataStore* db, domain::Id lsid, domain::Id lcid,
                      PublishContext* ctx) {
  std::vector<domain::Session>& ssns = db->ssns();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].lsid != lsid) {
      continue;
    }
    domain::ActivityVersion* ver = nullptr;
    std::vector<domain::ActivityVersion>& vers = db->activity_versions();
    for (std::size_t j = 0; j < vers.size(); ++j) {
      if (vers[j].id == ssns[i].vid) {
        ver = &vers[j];
        break;
      }
    }
    if (ver == nullptr || ver->st != domain::VersionStatus::kCurrentPublished) {
      continue;
    }
    domain::Activity* act = nullptr;
    std::vector<domain::Activity>& acts = db->acts();
    for (std::size_t j = 0; j < acts.size(); ++j) {
      if (acts[j].id == ver->aid) {
        act = &acts[j];
        break;
      }
    }
    if (act == nullptr || act->st != domain::ActivityStatus::kPublished) {
      continue;
    }
    domain::CategoryVersion* cat = nullptr;
    std::vector<domain::CategoryVersion>& cats = db->category_versions();
    for (std::size_t j = 0; j < cats.size(); ++j) {
      if (cats[j].vid == ver->id && cats[j].lcid == lcid) {
        cat = &cats[j];
        break;
      }
    }
    if (cat == nullptr || LogicalCategoryCancelled(db, lcid)) {
      continue;
    }
    ctx->ver = ver;
    ctx->act = act;
    ctx->ssn = &ssns[i];
    ctx->cat = cat;
    return true;
  }
  return false;
}

bool LogicalSessionCancelled(infra::DataStore* db, domain::Id lsid) {
  std::vector<domain::LogicalSession>& ssns = db->logical_sessions();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].id == lsid) {
      return ssns[i].cncl;
    }
  }
  return true;
}

bool HasActiveRegistration(const std::vector<domain::Registration>& xs,
                           domain::Id uid, domain::Id lsid, domain::Id lcid) {
  for (std::size_t i = 0; i < xs.size(); ++i) {
    const domain::Registration& x = xs[i];
    if (x.uid == uid && x.lsid == lsid && x.lcid == lcid &&
        (x.st == domain::RegistrationStatus::kRegistered ||
         x.st == domain::RegistrationStatus::kCheckedIn ||
         x.st == domain::RegistrationStatus::kWaitlisted)) {
      return true;
    }
  }
  return false;
}

bool HasWaitlisted(const std::vector<domain::Registration>& xs, domain::Id lsid,
                   domain::Id lcid) {
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].lsid == lsid && xs[i].lcid == lcid &&
        xs[i].st == domain::RegistrationStatus::kWaitlisted) {
      return true;
    }
  }
  return false;
}

}

RegistrationService::RegistrationService(infra::DataStore* db, RoleService* rls,
                                         AuditService* aud,
                                         NotificationService* ntfs,
                                         TicketService* tkts)
    : db_(db), rls_(rls), aud_(aud), ntfs_(ntfs), tkts_(tkts) {}

std::string RegistrationService::IdempotencyResult(const std::string& ky,
                                                   const std::string& reqH,
                                                   bool* fnd) {
  *fnd = false;
  if (ky.empty()) {
    return "";
  }
  std::vector<domain::IdempotencyRecord>& recs = db_->idempotency_records();
  for (std::size_t i = 0; i < recs.size(); ++i) {
    if (recs[i].ky != ky) {
      continue;
    }
    *fnd = true;
    if (recs[i].reqH != reqH) {
      return "FAIL|幂等键已被其他报名请求使用";
    }
    return recs[i].res;
  }
  return "";
}

ServiceResult RegistrationService::Register(domain::Id uid, domain::Id lsid,
                                            domain::Id lcid,
                                            const std::string& idemKy,
                                            std::string* det) {
  if (!rls_->HasRole(uid, domain::Role::kOrdinaryParticipant)) {
    return ServiceResult::Fail("仅普通参与者可以报名");
  }
  const std::string reqH = std::to_string(uid) + ":" + std::to_string(lsid) +
                           ":" + std::to_string(lcid);
  bool fnd = false;
  const std::string cch = IdempotencyResult(idemKy, reqH, &fnd);
  if (fnd) {
    const bool ok = cch.compare(0, 3, "OK|") == 0;
    *det = cch.substr(3);
    aud_->Log(uid, "报名幂等命中", domain::AuditResult::kSuccess,
              "键 " + idemKy);
    if (ok) {
      return ServiceResult::Ok(*det);
    }
    return ServiceResult::Fail(*det);
  }

  PublishContext ctx;
  if (!ResolvePublished(db_, lsid, lcid, &ctx)) {
    return ServiceResult::Fail("未找到已发布的场次与类别组合，请检查 ID");
  }
  if (LogicalSessionCancelled(db_, lsid)) {
    return ServiceResult::Fail("该场次已取消");
  }
  const domain::Timestamp ts = domain::Now();
  if (ts < ctx.ssn->regBeg) {
    return ServiceResult::Fail("报名尚未开始");
  }
  if (ts > ctx.ssn->regEndTs) {
    return ServiceResult::Fail("报名已截止");
  }
  if (ts > ctx.ssn->begTs) {
    return ServiceResult::Fail("场次已开始，无法报名");
  }
  if (HasActiveRegistration(db_->regs(), uid, lsid, lcid)) {
    return ServiceResult::Fail("您已报名或已在候补队列中");
  }

  domain::Registration reg;
  reg.id = db_->NextId("registration");
  reg.uid = uid;
  reg.lsid = lsid;
  reg.lcid = lcid;
  reg.vid = ctx.ver->id;
  reg.crtTs = ts;
  reg.cnclTs = 0;

  const int rem = RemainingSeats(lsid, lcid);
  const bool preWl = HasWaitlisted(db_->regs(), lsid, lcid);
  ServiceResult res;
  if (rem > 0 && !preWl) {
    reg.st = domain::RegistrationStatus::kRegistered;
    reg.wlSeq = 0;
    db_->regs().push_back(reg);
    std::string cd;
    tkts_->Issue(reg.id, &cd);
    *det = "报名成功，凭证码：" + cd;
    res = ServiceResult::Ok(*det);
    aud_->Log(uid, "报名", domain::AuditResult::kSuccess,
              ctx.act->ttl + " 凭证 " + cd);
  } else {
    domain::Id maxSeq = 0;
    const std::vector<domain::Registration>& xs = db_->regs();
    for (std::size_t i = 0; i < xs.size(); ++i) {
      if (xs[i].lsid == lsid && xs[i].lcid == lcid && xs[i].wlSeq > maxSeq) {
        maxSeq = xs[i].wlSeq;
      }
    }
    reg.st = domain::RegistrationStatus::kWaitlisted;
    reg.wlSeq = maxSeq + 1;
    db_->regs().push_back(reg);
    *det = "名额已满，已进入候补队列，候补序号 " + std::to_string(reg.wlSeq);
    res = ServiceResult::Ok(*det);
    aud_->Log(uid, "进入候补", domain::AuditResult::kSuccess,
              ctx.act->ttl + " 序号 " + std::to_string(reg.wlSeq));
  }

  if (!idemKy.empty()) {
    domain::IdempotencyRecord rec;
    rec.ky = idemKy;
    rec.op = "register";
    rec.reqH = reqH;
    rec.res = (res.ok ? "OK|" : "FAIL|") + *det;
    rec.crtTs = ts;
    db_->idempotency_records().push_back(rec);
  }
  return res;
}

ServiceResult RegistrationService::Cancel(domain::Id uid, domain::Id rid,
                                          std::string* det) {
  domain::Registration* reg = Find(rid);
  if (reg == nullptr) {
    return ServiceResult::Fail("报名记录不存在");
  }
  if (reg->uid != uid) {
    return ServiceResult::Fail("只能取消自己的报名");
  }
  if (reg->st == domain::RegistrationStatus::kCancelled) {
    return ServiceResult::Fail("报名已取消");
  }
  if (reg->st == domain::RegistrationStatus::kCheckedIn) {
    return ServiceResult::Fail("已签到，不能取消报名");
  }
  const domain::Id sid = reg->lsid;
  const domain::Id cid = reg->lcid;
  const bool wasReg = reg->st == domain::RegistrationStatus::kRegistered;
  reg->st = domain::RegistrationStatus::kCancelled;
  reg->cnclTs = domain::Now();
  if (wasReg) {
    tkts_->Void(rid);
  }
  aud_->Log(uid, "取消报名", domain::AuditResult::kSuccess,
            "报名 " + std::to_string(rid));
  *det = "报名已取消";
  if (wasReg) {
    std::string prm;
    if (PromoteOne(sid, cid, &prm)) {
      *det += "；" + prm;
    }
  }
  return ServiceResult::Ok(*det);
}

bool RegistrationService::PromoteOne(domain::Id lsid, domain::Id lcid,
                                     std::string* det) {
  if (RemainingSeats(lsid, lcid) <= 0) {
    return false;
  }
  domain::Registration* cand = nullptr;
  std::vector<domain::Registration>& xs = db_->regs();
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].lsid != lsid || xs[i].lcid != lcid ||
        xs[i].st != domain::RegistrationStatus::kWaitlisted) {
      continue;
    }
    if (cand == nullptr || xs[i].wlSeq < cand->wlSeq) {
      cand = &xs[i];
    }
  }
  if (cand == nullptr) {
    return false;
  }
  cand->st = domain::RegistrationStatus::kRegistered;
  cand->wlSeq = 0;
  std::string cd;
  tkts_->Issue(cand->id, &cd);
  ntfs_->Notify(cand->uid, domain::NotificationType::kPromoted,
                "您已从候补递补成功，报名#" + std::to_string(cand->id) +
                    " 凭证码：" + cd);
  aud_->Log(0, "候补递补", domain::AuditResult::kSuccess,
            "报名 " + std::to_string(cand->id) + " 递补成功");
  *det = "候补用户已自动递补，凭证码 " + cd;
  return true;
}

std::vector<domain::Registration> RegistrationService::RegistrationsOf(
    domain::Id uid) {
  std::vector<domain::Registration> res;
  std::vector<domain::Registration>& xs = db_->regs();
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].uid == uid) {
      res.push_back(xs[i]);
    }
  }
  return res;
}

domain::Registration* RegistrationService::Find(domain::Id rid) {
  std::vector<domain::Registration>& xs = db_->regs();
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id == rid) {
      return &xs[i];
    }
  }
  return nullptr;
}

int RegistrationService::SeatCount(domain::Id lsid, domain::Id lcid) {
  int n = 0;
  const std::vector<domain::Registration>& xs = db_->regs();
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].lsid != lsid || xs[i].lcid != lcid) {
      continue;
    }
    if (xs[i].st == domain::RegistrationStatus::kRegistered ||
        xs[i].st == domain::RegistrationStatus::kCheckedIn) {
      ++n;
    }
  }
  return n;
}

int RegistrationService::RemainingSeats(domain::Id lsid, domain::Id lcid) {
  PublishContext ctx;
  if (!ResolvePublished(db_, lsid, lcid, &ctx)) {
    return 0;
  }
  int ssnUsed = 0;
  const std::vector<domain::Registration>& xs = db_->regs();
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].lsid == lsid &&
        (xs[i].st == domain::RegistrationStatus::kRegistered ||
         xs[i].st == domain::RegistrationStatus::kCheckedIn)) {
      ++ssnUsed;
    }
  }
  const int catRem = ctx.cat->cap - SeatCount(lsid, lcid);
  const int ssnRem = ctx.ssn->cap - ssnUsed;
  if (catRem < ssnRem) {
    return catRem < 0 ? 0 : catRem;
  }
  return ssnRem < 0 ? 0 : ssnRem;
}

}
}
