#include <filesystem>
#include <iostream>
#include <string>

#include "app/app_context.h"
#include "app/seed.h"
#include "domain/datetime.h"
#include "infra/data_store.h"
#include "service/activity_service.h"
#include "service/audit_service.h"
#include "service/auth_service.h"
#include "service/notification_service.h"
#include "service/registration_service.h"
#include "service/role_service.h"
#include "service/session_service.h"
#include "service/stats_service.h"
#include "service/ticket_service.h"

namespace {

using campus::domain::Id;

int badN = 0;

void Check(bool ok, const std::string& nm) {
  if (ok) {
    std::cout << "[PASS] " << nm << std::endl;
    return;
  }
  ++badN;
  std::cerr << "[FAIL] " << nm << std::endl;
}

struct Fx {
  campus::infra::DataStore db;
  campus::service::AuditService aud{&db};
  campus::service::NotificationService ntfs{&db};
  campus::service::RoleService rls{&db, &aud};
  campus::service::AuthService auth{&db, &aud, &rls};
  campus::service::TicketService tkts{&db, &rls, &aud};
  campus::service::SessionService ssns{&db, &rls, &aud, &ntfs, &tkts};
  campus::service::ActivityService acts{&db, &rls, &aud, &ntfs, &tkts};
  campus::service::RegistrationService regs{&db, &rls, &aud, &ntfs, &tkts};
  campus::service::StatsService sts{&db, &rls};
  campus::app::AppContext ctx;

  Fx() {
    ctx.db = &db;
    ctx.aud = &aud;
    ctx.ntfs = &ntfs;
    ctx.rls = &rls;
    ctx.auth = &auth;
    ctx.tkts = &tkts;
    ctx.ssns = &ssns;
    ctx.acts = &acts;
    ctx.regs = &regs;
    ctx.sts = &sts;
    campus::app::SeedDemoData(&ctx);
  }

  Id Uid(const std::string& acct) {
    auto* u = auth.FindByAccount(acct);
    return u == nullptr ? 0 : u->id;
  }
};

void TestLoginLock() {
  Fx f;
  Id uid = 0;
  for (int i = 0; i < 5; ++i) {
    Check(!f.auth.Login("student2", "bad", &uid).ok, "wrong password rejected");
  }
  Check(!f.auth.Login("student2", "123456", &uid).ok,
        "locked account rejected");
}

void TestWaitlist() {
  Fx f;
  const auto x = f.db.regs().front();
  std::string det;
  auto res = f.regs.Register(f.Uid("student1"), x.lsid, x.lcid, "wl-1", &det);
  Check(res.ok, "waitlist request accepted");
  auto xs = f.regs.RegistrationsOf(f.Uid("student1"));
  auto* reg = xs.empty() ? nullptr : f.regs.Find(xs.back().id);
  Check(reg != nullptr &&
            reg->st == campus::domain::RegistrationStatus::kWaitlisted,
        "full category creates waitlist");
  res = f.regs.Cancel(x.uid, x.id, &det);
  Check(res.ok && reg != nullptr &&
            reg->st == campus::domain::RegistrationStatus::kRegistered,
        "cancel promotes first waitlist entry");
  Check(reg != nullptr && f.tkts.FindByRegistration(reg->id) != nullptr,
        "promotion issues ticket");
}

void TestCategoryCancel() {
  Fx f;
  const auto x = f.db.regs().front();
  auto res = f.ssns.CancelCategory(f.Uid("organizer"), x.lcid);
  Check(res.ok, "owner cancels category");
  std::string det;
  res = f.regs.Register(f.Uid("student1"), x.lsid, x.lcid, "cat-off", &det);
  Check(!res.ok, "cancelled category rejects registration");
  auto* reg = f.regs.Find(x.id);
  Check(reg != nullptr &&
            reg->st == campus::domain::RegistrationStatus::kCancelled,
        "category cancel revokes registration");
}

void TestReviewPublish() {
  Fx f;
  auto xs = f.acts.PendingVersions();
  Check(!xs.empty(), "pending version exists");
  if (xs.empty()) {
    return;
  }
  auto res = f.acts.Review(f.Uid("admin"), xs.front().id, true, "");
  Check(res.ok, "admin approves version");
  res = f.acts.Publish(f.Uid("organizer"), xs.front().id);
  Check(res.ok, "owner publishes approved version");
  Check(f.acts.CurrentVersion(xs.front().aid) != nullptr,
        "published version becomes current");
}

void TestCheckIn() {
  Fx f;
  campus::domain::Registration* reg = nullptr;
  for (auto& x : f.db.regs()) {
    auto* t = f.tkts.FindByRegistration(x.id);
    if (t == nullptr) {
      continue;
    }
    for (const auto& s : f.db.ssns()) {
      if (s.lsid == x.lsid &&
          f.tkts.InCheckinWindow(s, campus::domain::Now())) {
        reg = &x;
        break;
      }
    }
    if (reg != nullptr) {
      break;
    }
  }
  Check(reg != nullptr, "check-in sample exists");
  if (reg == nullptr) {
    return;
  }
  auto* t = f.tkts.FindByRegistration(reg->id);
  auto res = f.tkts.CheckIn(f.Uid("inspector"), t->cd);
  Check(res.ok, "authorized inspector checks in");
  Check(!f.tkts.CheckIn(f.Uid("inspector"), t->cd).ok,
        "duplicate check-in rejected");
}

void TestRosterAccess() {
  Fx f;
  Id uid = 0;
  auto res = f.auth.CreateAccount(f.Uid("admin"), "org2", "组织者二", "123456",
                                  campus::domain::Role::kOrganizer, &uid);
  Check(res.ok, "second organizer created");
  const Id aid = f.db.acts().front().id;
  Check(f.sts.QueryStats(uid, aid).empty(),
        "other organizer cannot query stats");
  std::string sum;
  res = f.sts.ExportRoster(uid, aid, "blocked.csv", &sum);
  Check(!res.ok && !std::filesystem::exists("blocked.csv"),
        "other organizer cannot export roster");
  Check(!f.sts.QueryStats(f.Uid("admin"), aid).empty(),
        "admin can query stats");
}

void TestPersistence() {
  Fx f;
  const auto dir = std::filesystem::temp_directory_path() /
                   ("campus-test-" + std::to_string(campus::domain::Now()));
  f.db.Save(dir.string());
  campus::infra::DataStore db;
  db.Load(dir.string());
  Check(db.usrs().size() == f.db.usrs().size(), "users reload from csv");
  Check(db.regs().size() == f.db.regs().size(),
        "registrations reload from csv");
  std::filesystem::remove_all(dir);
}

}

int main() {
  TestLoginLock();
  TestWaitlist();
  TestCategoryCancel();
  TestReviewPublish();
  TestCheckIn();
  TestRosterAccess();
  TestPersistence();
  std::cout << "Failures: " << badN << std::endl;
  return badN == 0 ? 0 : 1;
}
