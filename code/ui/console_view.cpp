

#include "ui/console_view.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "app/app_context.h"
#include "domain/datetime.h"
#include "domain/entities.h"
#include "infra/data_store.h"
#include "service/activity_service.h"
#include "service/auth_service.h"
#include "service/notification_service.h"
#include "service/registration_service.h"
#include "service/role_service.h"
#include "service/service_result.h"
#include "service/session_service.h"
#include "service/ticket_service.h"
#include "ui/console_io.h"

namespace campus {
namespace ui {

ConsoleView::ConsoleView(app::AppContext* ctx) : ctx_(ctx) {}

void ConsoleView::PrintBanner() {
  std::cout << "=================================================="
            << std::endl;
  std::cout << "        校园活动票务与报名系统（控制台版）        "
            << std::endl;
  std::cout << "  四角色：普通参与者 / 组织者 / 检票员 / 管理员   "
            << std::endl;
  std::cout << "=================================================="
            << std::endl;
}

void ConsoleView::Run() {
  PrintBanner();
  LoginLoop();
  ctx_->db->Save(ctx_->ddir);
  io::PrintInfo("数据已保存，系统退出。");
}

bool ConsoleView::EnsurePasswordChanged(domain::Id uid) {
  domain::User* usr = ctx_->auth->FindUser(uid);
  if (usr == nullptr) {
    return false;
  }
  if (!usr->forcePwd) {
    return true;
  }
  io::PrintInfo("首次登录必须修改初始密码。");
  while (true) {
    const std::string oldPwd = io::ReadLine("原密码: ");
    const std::string newPwd = io::ReadLine("新密码（至少 6 位）: ");
    const service::ServiceResult res =
        ctx_->auth->ChangePassword(uid, oldPwd, newPwd);
    if (res.ok) {
      io::PrintOk(res.msg);
      return true;
    }
    io::PrintFail(res.msg);
    if (!io::Confirm("是否继续修改密码")) {
      return false;
    }
  }
}

domain::Role ConsoleView::ChooseRole(const std::vector<domain::Role>& rls) {
  io::PrintInfo("该账号拥有多个角色，请选择本次登录身份：");
  for (std::size_t i = 0; i < rls.size(); ++i) {
    std::cout << "  " << (i + 1) << ". " << domain::ToString(rls[i])
              << std::endl;
  }
  while (true) {
    const int ch = io::ReadInt("请选择: ");
    if (ch >= 1 && ch <= static_cast<int>(rls.size())) {
      return rls[static_cast<std::size_t>(ch - 1)];
    }
    io::PrintFail("无效选项");
  }
}

void ConsoleView::LoginLoop() {
  while (true) {
    const std::string acct = io::ReadLine("\n账号（输入 0 退出系统）: ");
    if (acct == "0") {
      return;
    }
    const std::string pwd = io::ReadLine("密码: ");
    domain::Id uid = 0;
    const service::ServiceResult res = ctx_->auth->Login(acct, pwd, &uid);
    if (!res.ok) {
      io::PrintFail(res.msg);
      continue;
    }
    io::PrintOk(res.msg);
    if (!EnsurePasswordChanged(uid)) {
      continue;
    }
    const std::vector<domain::Role> rls = ctx_->rls->RolesOf(uid);
    if (rls.empty()) {
      io::PrintFail("该账号尚未分配角色，请联系管理员");
      continue;
    }
    domain::Role rl = rls[0];
    if (rls.size() > 1) {
      rl = ChooseRole(rls);
    }
    Dispatch(rl, uid);
    ctx_->db->Save(ctx_->ddir);
  }
}

void ConsoleView::Dispatch(domain::Role rl, domain::Id uid) {
  switch (rl) {
    case domain::Role::kOrdinaryParticipant:
      ParticipantMenu(uid);
      break;
    case domain::Role::kOrganizer:
      OrganizerMenu(uid);
      break;
    case domain::Role::kInspector:
      InspectorMenu(uid);
      break;
    case domain::Role::kAdministrator:
      AdminMenu(uid);
      break;
  }
  io::PrintInfo("已退出当前角色菜单。");
}

void ConsoleView::ParticipantMenu(domain::Id uid) {
  while (true) {
    std::cout << "\n--- 参与者菜单 ---" << std::endl;
    std::cout << "1. 浏览已发布活动与场次" << std::endl;
    std::cout << "2. 报名（选择场次与类别）" << std::endl;
    std::cout << "3. 取消报名" << std::endl;
    std::cout << "4. 查看我的报名" << std::endl;
    std::cout << "5. 查看我的验票凭证" << std::endl;
    std::cout << "6. 查看未读通知" << std::endl;
    std::cout << "0. 退出登录" << std::endl;
    const int ch = io::ReadInt("请选择: ");
    switch (ch) {
      case 1:
        ShowPublishedActivities();
        break;
      case 2:
        DoRegister(uid);
        break;
      case 3:
        DoCancelRegistration(uid);
        break;
      case 4:
        ShowMyRegistrations(uid);
        break;
      case 5:
        ShowMyTickets(uid);
        break;
      case 6:
        ShowNotifications(uid);
        break;
      case 0:
        return;
      default:
        io::PrintFail("无效选项");
        break;
    }
    ctx_->db->Save(ctx_->ddir);
  }
}

void ConsoleView::ShowPublishedActivities() {
  const std::vector<domain::Activity> acts = ctx_->acts->AllActivities();
  bool fnd = false;
  for (std::size_t i = 0; i < acts.size(); ++i) {
    if (acts[i].st != domain::ActivityStatus::kPublished) {
      continue;
    }
    domain::ActivityVersion* ver = ctx_->acts->CurrentVersion(acts[i].id);
    if (ver == nullptr) {
      continue;
    }
    fnd = true;
    std::cout << "\n[活动#" << acts[i].id << "] " << acts[i].ttl
              << "（当前版本#" << ver->id << "）" << std::endl;
    ShowVersionDetail(ver->id);
  }
  if (!fnd) {
    io::PrintInfo("当前没有已发布的活动。");
  }
}

void ConsoleView::ShowVersionDetail(domain::Id vid) {
  const std::vector<domain::Session> ssns = ctx_->ssns->SessionsOfVersion(vid);
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    const domain::Session& ssn = ssns[i];
    domain::LogicalSession* lgl = ctx_->ssns->FindLogicalSession(ssn.lsid);
    std::cout << "  场次#" << ssn.lsid << " " << ssn.loc << " "
              << domain::FormatTime(ssn.begTs) << " ~ "
              << domain::FormatTime(ssn.endTs) << " 报名截止 "
              << domain::FormatTime(ssn.regEndTs) << " 容量 " << ssn.cap;
    if (lgl != nullptr && lgl->cncl) {
      std::cout << " [已取消]";
    }
    std::cout << std::endl;
  }
  const std::vector<domain::CategoryVersion> cats =
      ctx_->ssns->CategoriesOfVersion(vid);
  for (std::size_t i = 0; i < cats.size(); ++i) {
    std::cout << "    类别#" << cats[i].lcid << " " << cats[i].nm << " 名额 "
              << cats[i].cap << std::endl;
  }
}

void ConsoleView::DoRegister(domain::Id uid) {
  ShowPublishedActivities();
  const domain::Id sid = io::ReadId("请输入场次逻辑 ID: ");
  const domain::Id cid = io::ReadId("请输入类别逻辑 ID: ");
  const std::string ky =
      io::ReadLine("幂等键（可空，重复提交演示时填入相同值）: ");
  std::string det;
  const service::ServiceResult res =
      ctx_->regs->Register(uid, sid, cid, ky, &det);
  if (res.ok) {
    io::PrintOk(res.msg);
    io::PrintInfo("剩余名额：" +
                  std::to_string(ctx_->regs->RemainingSeats(sid, cid)));
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::DoCancelRegistration(domain::Id uid) {
  ShowMyRegistrations(uid);
  const domain::Id rid = io::ReadId("请输入要取消的报名 ID: ");
  if (!io::Confirm("确认取消该报名")) {
    io::PrintInfo("已放弃操作。");
    return;
  }
  std::string det;
  const service::ServiceResult res = ctx_->regs->Cancel(uid, rid, &det);
  if (res.ok) {
    io::PrintOk(res.msg);
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::ShowMyRegistrations(domain::Id uid) {
  const std::vector<domain::Registration> xs = ctx_->regs->RegistrationsOf(uid);
  if (xs.empty()) {
    io::PrintInfo("暂无报名记录。");
    return;
  }
  for (std::size_t i = 0; i < xs.size(); ++i) {
    std::cout << "  " << ctx_->tkts->Describe(xs[i].id) << std::endl;
  }
}

void ConsoleView::ShowMyTickets(domain::Id uid) {
  const std::vector<domain::Registration> xs = ctx_->regs->RegistrationsOf(uid);
  bool fnd = false;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].st == domain::RegistrationStatus::kRegistered ||
        xs[i].st == domain::RegistrationStatus::kCheckedIn) {
      std::cout << "  " << ctx_->tkts->Describe(xs[i].id) << std::endl;
      fnd = true;
    }
  }
  if (!fnd) {
    io::PrintInfo("暂无有效凭证。");
  }
}

void ConsoleView::ShowNotifications(domain::Id uid) {
  const std::vector<domain::Notification> xs = ctx_->ntfs->Unread(uid);
  if (xs.empty()) {
    io::PrintInfo("没有未读通知。");
    return;
  }
  for (std::size_t i = 0; i < xs.size(); ++i) {
    std::cout << "  #" << xs[i].id << " [" << domain::ToString(xs[i].typ)
              << "] " << domain::FormatTime(xs[i].crtTs) << " " << xs[i].cnt
              << std::endl;
  }
  if (io::Confirm("是否全部标记为已读")) {
    const int n = ctx_->ntfs->MarkAllRead(uid);
    io::PrintInfo("已标记 " + std::to_string(n) + " 条通知为已读。");
  }
}

}
}
