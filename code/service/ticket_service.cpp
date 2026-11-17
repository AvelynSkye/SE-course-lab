

#include "service/ticket_service.h"

#include <cstddef>
#include <string>
#include <vector>

#include "domain/datetime.h"
#include "infra/csv_util.h"
#include "infra/data_store.h"
#include "infra/security.h"
#include "service/audit_service.h"
#include "service/role_service.h"

namespace campus {
namespace service {

TicketService::TicketService(infra::DataStore* db, RoleService* rls,
                             AuditService* aud)
    : db_(db), rls_(rls), aud_(aud) {}

ServiceResult TicketService::Issue(domain::Id rid, std::string* cd) {
  domain::Registration* reg = FindRegistration(rid);
  if (reg == nullptr) {
    return ServiceResult::Fail("报名记录不存在");
  }
  std::vector<domain::Ticket>& tkts = db_->tkts();
  for (std::size_t i = 0; i < tkts.size(); ++i) {
    if (tkts[i].rid == rid) {
      tkts[i].st = domain::TicketStatus::kInvalid;
    }
  }
  domain::Ticket tkt;
  tkt.id = db_->NextId("ticket");
  tkt.rid = rid;
  tkt.cd = infra::TicketCodeGenerator::Generate();
  tkt.st = domain::TicketStatus::kValid;
  tkt.issTs = domain::Now();
  tkts.push_back(tkt);
  if (cd != nullptr) {
    *cd = tkt.cd;
  }
  return ServiceResult::Ok(tkt.cd);
}

ServiceResult TicketService::Void(domain::Id rid) {
  bool fnd = false;
  std::vector<domain::Ticket>& tkts = db_->tkts();
  for (std::size_t i = 0; i < tkts.size(); ++i) {
    if (tkts[i].rid == rid && tkts[i].st == domain::TicketStatus::kValid) {
      tkts[i].st = domain::TicketStatus::kInvalid;
      fnd = true;
    }
  }
  if (!fnd) {
    return ServiceResult::Fail("没有可失效的凭证");
  }
  return ServiceResult::Ok("凭证已失效");
}

domain::Ticket* TicketService::FindByCode(const std::string& cd) {
  std::vector<domain::Ticket>& tkts = db_->tkts();
  for (std::size_t i = 0; i < tkts.size(); ++i) {
    if (tkts[i].cd == cd) {
      return &tkts[i];
    }
  }
  return nullptr;
}

domain::Ticket* TicketService::FindByRegistration(domain::Id rid) {
  std::vector<domain::Ticket>& tkts = db_->tkts();
  for (std::size_t i = 0; i < tkts.size(); ++i) {
    if (tkts[i].rid == rid && tkts[i].st == domain::TicketStatus::kValid) {
      return &tkts[i];
    }
  }
  return nullptr;
}

domain::Registration* TicketService::FindRegistration(domain::Id rid) {
  std::vector<domain::Registration>& regs = db_->regs();
  for (std::size_t i = 0; i < regs.size(); ++i) {
    if (regs[i].id == rid) {
      return &regs[i];
    }
  }
  return nullptr;
}

bool TicketService::InCheckinWindow(const domain::Session& ssn,
                                    domain::Timestamp ts) {
  const domain::Timestamp opn = domain::AddMinutes(ssn.begTs, -30);
  return ts >= opn && ts <= ssn.endTs;
}

ServiceResult TicketService::CheckIn(domain::Id inspId, const std::string& cd) {
  if (!rls_->HasRole(inspId, domain::Role::kInspector)) {
    return ServiceResult::Fail("仅检票员可以执行验票");
  }
  const std::string trimmed = infra::Trim(cd);
  domain::Ticket* tkt = FindByCode(trimmed);
  if (tkt == nullptr) {
    aud_->Log(inspId, "验票", domain::AuditResult::kFailure,
              "凭证码不存在：" + trimmed);
    return ServiceResult::Fail("凭证码无效");
  }
  if (tkt->st != domain::TicketStatus::kValid) {
    aud_->Log(inspId, "验票", domain::AuditResult::kFailure,
              "凭证已失效：" + trimmed);
    return ServiceResult::Fail("凭证已失效，可能报名已取消");
  }
  domain::Registration* reg = FindRegistration(tkt->rid);
  if (reg == nullptr) {
    return ServiceResult::Fail("报名记录缺失");
  }
  if (reg->st == domain::RegistrationStatus::kCancelled) {
    return ServiceResult::Fail("报名已取消，不允许入场");
  }
  if (reg->st == domain::RegistrationStatus::kCheckedIn) {
    aud_->Log(inspId, "验票", domain::AuditResult::kFailure,
              "重复签到，报名 " + std::to_string(reg->id));
    return ServiceResult::Fail("重复签到：该凭证已使用");
  }
  if (reg->st != domain::RegistrationStatus::kRegistered) {
    return ServiceResult::Fail("当前报名状态不可入场");
  }
  if (!rls_->HasValidGrant(inspId, reg->lsid)) {
    aud_->Log(inspId, "验票", domain::AuditResult::kFailure,
              "无场次授权 " + std::to_string(reg->lsid));
    return ServiceResult::Fail("您没有该场次的检票授权");
  }
  domain::Session* ssn = nullptr;
  std::vector<domain::Session>& ssns = db_->ssns();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].lsid == reg->lsid && ssns[i].vid == reg->vid) {
      ssn = &ssns[i];
      break;
    }
  }
  if (ssn == nullptr) {
    return ServiceResult::Fail("场次信息缺失");
  }
  if (!InCheckinWindow(*ssn, domain::Now())) {
    aud_->Log(inspId, "验票", domain::AuditResult::kFailure, "不在验票窗口");
    return ServiceResult::Fail(
        "不在验票时间窗口内（场次开始前 30 分钟至结束）");
  }
  for (std::size_t i = 0; i < db_->checkin_events().size(); ++i) {
    if (db_->checkin_events()[i].rid == reg->id) {
      return ServiceResult::Fail("重复签到：该报名已有签到记录");
    }
  }
  domain::CheckinEvent ev;
  ev.id = db_->NextId("checkin");
  ev.rid = reg->id;
  ev.chkTs = domain::Now();
  db_->checkin_events().push_back(ev);
  reg->st = domain::RegistrationStatus::kCheckedIn;

  std::string atd = std::to_string(reg->uid);
  std::vector<domain::User>& usrs = db_->usrs();
  for (std::size_t i = 0; i < usrs.size(); ++i) {
    if (usrs[i].id == reg->uid) {
      atd = usrs[i].nm;
      break;
    }
  }
  aud_->Log(inspId, "验票", domain::AuditResult::kSuccess,
            "报名 " + std::to_string(reg->id) + " 签到成功");
  return ServiceResult::Ok("签到成功：" + atd + " / " + ssn->loc);
}

std::string TicketService::Describe(domain::Id rid) {
  domain::Registration* reg = FindRegistration(rid);
  if (reg == nullptr) {
    return "报名记录不存在";
  }
  std::string loc = "-";
  std::string tmTxt = "-";
  std::vector<domain::Session>& ssns = db_->ssns();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].lsid == reg->lsid && ssns[i].vid == reg->vid) {
      loc = ssns[i].loc;
      tmTxt = domain::FormatTime(ssns[i].begTs);
      break;
    }
  }
  std::string cd = "无";
  domain::Ticket* tkt = FindByRegistration(rid);
  if (tkt != nullptr) {
    cd = tkt->cd;
  }
  return "报名#" + std::to_string(reg->id) + " " + loc + " " + tmTxt +
         " 状态:" + domain::ToString(reg->st) + " 凭证:" + cd;
}

}
}
