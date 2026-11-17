

#include "service/stats_service.h"

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "domain/datetime.h"
#include "domain/entities.h"
#include "infra/csv_util.h"
#include "infra/data_store.h"
#include "service/role_service.h"

namespace campus {
namespace service {

namespace {

domain::Activity* FindActivity(infra::DataStore* db, domain::Id aid) {
  std::vector<domain::Activity>& acts = db->acts();
  for (std::size_t i = 0; i < acts.size(); ++i) {
    if (acts[i].id == aid) {
      return &acts[i];
    }
  }
  return nullptr;
}

bool CanAccessActivity(RoleService* rls, domain::Id uid,
                       const domain::Activity& act) {
  if (rls->HasRole(uid, domain::Role::kAdministrator)) {
    return true;
  }
  return rls->HasRole(uid, domain::Role::kOrganizer) && act.crtId == uid;
}

std::string FindText(const std::vector<domain::User>& usrs, domain::Id uid,
                     bool wantNm) {
  for (std::size_t i = 0; i < usrs.size(); ++i) {
    if (usrs[i].id == uid) {
      return wantNm ? usrs[i].nm : usrs[i].acct;
    }
  }
  return "-";
}

std::string SessionText(infra::DataStore* db, const domain::Registration& reg) {
  std::vector<domain::Session>& ssns = db->ssns();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].lsid == reg.lsid && ssns[i].vid == reg.vid) {
      return ssns[i].loc + " " + domain::FormatTime(ssns[i].begTs);
    }
  }
  return "-";
}

std::string CategoryText(infra::DataStore* db,
                         const domain::Registration& reg) {
  std::vector<domain::CategoryVersion>& cats = db->category_versions();
  for (std::size_t i = 0; i < cats.size(); ++i) {
    if (cats[i].lcid == reg.lcid && cats[i].vid == reg.vid) {
      return cats[i].nm;
    }
  }
  return "-";
}

std::string TicketText(infra::DataStore* db, domain::Id rid) {
  std::vector<domain::Ticket>& tkts = db->tkts();
  for (std::size_t i = 0; i < tkts.size(); ++i) {
    if (tkts[i].rid == rid && tkts[i].st == domain::TicketStatus::kValid) {
      return tkts[i].cd;
    }
  }
  return "-";
}

std::string CheckinText(infra::DataStore* db, domain::Id rid) {
  std::vector<domain::CheckinEvent>& evs = db->checkin_events();
  for (std::size_t i = 0; i < evs.size(); ++i) {
    if (evs[i].rid == rid) {
      return domain::FormatTime(evs[i].chkTs);
    }
  }
  return "-";
}

}

std::string StatResult::ToString() const {
  char buf[64] = {0};
  std::snprintf(buf, sizeof(buf), "%.1f%%", attRt * 100.0);
  return "活动《" + ttl + "》：已报名 " + std::to_string(regN) + "，已签到 " +
         std::to_string(chkN) + "，候补 " + std::to_string(wlN) + "，已取消 " +
         std::to_string(cncl) + "，到场率 " + buf;
}

StatsService::StatsService(infra::DataStore* db, RoleService* rls)
    : db_(db), rls_(rls) {}

std::vector<StatResult> StatsService::QueryStats(domain::Id uid,
                                                 domain::Id aid) {
  std::vector<StatResult> results;
  domain::Activity* act = FindActivity(db_, aid);
  if (act == nullptr || !CanAccessActivity(rls_, uid, *act)) {
    return results;
  }
  domain::ActivityVersion* ver = nullptr;
  std::vector<domain::ActivityVersion>& vers = db_->activity_versions();
  for (std::size_t i = 0; i < vers.size(); ++i) {
    if (vers[i].aid == aid &&
        vers[i].st == domain::VersionStatus::kCurrentPublished) {
      ver = &vers[i];
      break;
    }
  }
  StatResult res;
  res.aid = aid;
  res.ttl = act->ttl;
  if (ver != nullptr) {
    std::vector<domain::Registration>& regs = db_->regs();
    for (std::size_t i = 0; i < regs.size(); ++i) {
      if (regs[i].vid != ver->id) {
        continue;
      }
      switch (regs[i].st) {
        case domain::RegistrationStatus::kRegistered:
          ++res.regN;
          break;
        case domain::RegistrationStatus::kCheckedIn:
          ++res.chkN;
          break;
        case domain::RegistrationStatus::kWaitlisted:
          ++res.wlN;
          break;
        case domain::RegistrationStatus::kCancelled:
          ++res.cncl;
          break;
      }
    }
  }
  const int total = res.regN + res.chkN;
  if (total > 0) {
    res.attRt = static_cast<double>(res.chkN) / static_cast<double>(total);
  }
  results.push_back(res);
  return results;
}

ServiceResult StatsService::ExportRoster(domain::Id uid, domain::Id aid,
                                         const std::string& pth,
                                         std::string* sum) {
  domain::Activity* act = FindActivity(db_, aid);
  if (act == nullptr) {
    return ServiceResult::Fail("活动不存在");
  }
  if (!CanAccessActivity(rls_, uid, *act)) {
    return ServiceResult::Fail("无权导出该活动名单");
  }
  domain::ActivityVersion* ver = nullptr;
  std::vector<domain::ActivityVersion>& vers = db_->activity_versions();
  for (std::size_t i = 0; i < vers.size(); ++i) {
    if (vers[i].aid == aid &&
        vers[i].st == domain::VersionStatus::kCurrentPublished) {
      ver = &vers[i];
      break;
    }
  }
  if (ver == nullptr) {
    return ServiceResult::Fail("该活动没有当前发布版本");
  }
  std::ofstream out(pth.c_str());
  if (!out.is_open()) {
    return ServiceResult::Fail("无法写入文件：" + pth);
  }
  std::vector<std::string> hdr;
  hdr.push_back("用户ID");
  hdr.push_back("姓名");
  hdr.push_back("账号");
  hdr.push_back("场次");
  hdr.push_back("类别");
  hdr.push_back("报名状态");
  hdr.push_back("凭证码");
  hdr.push_back("签到时间");
  out << infra::JoinCsv(hdr) << "\n";

  int exported = 0;
  std::vector<domain::Registration>& regs = db_->regs();
  for (std::size_t i = 0; i < regs.size(); ++i) {
    const domain::Registration& reg = regs[i];
    if (reg.vid != ver->id) {
      continue;
    }
    std::vector<std::string> x;
    x.push_back(std::to_string(reg.uid));
    x.push_back(FindText(db_->usrs(), reg.uid, true));
    x.push_back(FindText(db_->usrs(), reg.uid, false));
    x.push_back(SessionText(db_, reg));
    x.push_back(CategoryText(db_, reg));
    x.push_back(domain::ToString(reg.st));
    x.push_back(TicketText(db_, reg.id));
    x.push_back(CheckinText(db_, reg.id));
    out << infra::JoinCsv(x) << "\n";
    ++exported;
  }
  out.close();
  *sum = "已导出 " + std::to_string(exported) + " 条名单记录到 " + pth;
  return ServiceResult::Ok(*sum);
}

}
}
