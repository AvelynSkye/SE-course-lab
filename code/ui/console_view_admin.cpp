

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "app/app_context.h"
#include "domain/datetime.h"
#include "domain/entities.h"
#include "infra/data_store.h"
#include "service/activity_service.h"
#include "service/audit_service.h"
#include "service/auth_service.h"
#include "service/background_task_service.h"
#include "service/role_service.h"
#include "service/service_result.h"
#include "service/session_service.h"
#include "service/stats_service.h"
#include "service/ticket_service.h"
#include "ui/console_io.h"
#include "ui/console_view.h"

namespace campus {
namespace ui {

namespace {

bool ParseRoleChoice(int ch, domain::Role* rl) {
  switch (ch) {
    case 1:
      *rl = domain::Role::kOrdinaryParticipant;
      return true;
    case 2:
      *rl = domain::Role::kOrganizer;
      return true;
    case 3:
      *rl = domain::Role::kInspector;
      return true;
    case 4:
      *rl = domain::Role::kAdministrator;
      return true;
    default:
      return false;
  }
}

std::string JoinRoles(const std::vector<domain::Role>& rls) {
  std::string txt;
  for (std::size_t i = 0; i < rls.size(); ++i) {
    if (i > 0) {
      txt += "/";
    }
    txt += domain::ToString(rls[i]);
  }
  return txt.empty() ? "-" : txt;
}

}

void ConsoleView::AdminMenu(domain::Id uid) {
  while (true) {
    std::cout << "\n--- 管理员菜单 ---" << std::endl;
    std::cout << "1. 审核待审核活动版本" << std::endl;
    std::cout << "2. 用户列表" << std::endl;
    std::cout << "3. 创建账号" << std::endl;
    std::cout << "4. 分配角色" << std::endl;
    std::cout << "5. 启用 / 停用账号" << std::endl;
    std::cout << "6. 授予场次检票权限" << std::endl;
    std::cout << "7. 查询审计日志" << std::endl;
    std::cout << "8. 后台任务" << std::endl;
    std::cout << "9. 名单统计与导出" << std::endl;
    std::cout << "0. 退出登录" << std::endl;
    const int ch = io::ReadInt("请选择: ");
    switch (ch) {
      case 1:
        ReviewVersions(uid);
        break;
      case 2:
        ListUsers();
        break;
      case 3:
        CreateAccount(uid);
        break;
      case 4:
        AssignRole(uid);
        break;
      case 5:
        ChangeUserStatus(uid);
        break;
      case 6:
        GrantSession(uid);
        break;
      case 7:
        ShowAuditLogs();
        break;
      case 8:
        BackgroundTasks(uid);
        break;
      case 9:
        ShowStats(uid);
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

void ConsoleView::ReviewVersions(domain::Id uid) {
  const std::vector<domain::ActivityVersion> pnd =
      ctx_->acts->PendingVersions();
  if (pnd.empty()) {
    io::PrintInfo("当前没有待审核的版本。");
    return;
  }
  for (std::size_t i = 0; i < pnd.size(); ++i) {
    std::cout << "  " << ctx_->acts->DescribeVersion(pnd[i].id) << std::endl;
    ShowVersionDetail(pnd[i].id);
  }
  const domain::Id vid = io::ReadId("请输入要审核的版本 ID: ");
  const int ch = io::ReadInt("1 审核通过 / 2 审核驳回: ");
  std::string rsn;
  if (ch == 2) {
    rsn = io::ReadLine("请输入驳回原因: ");
  }
  const service::ServiceResult res = ctx_->acts->Review(uid, vid, ch == 1, rsn);
  if (res.ok) {
    io::PrintOk(res.msg);
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::ListUsers() {
  const std::vector<domain::User> usrs = ctx_->auth->AllUsers();
  for (std::size_t i = 0; i < usrs.size(); ++i) {
    std::cout << "  #" << usrs[i].id << " " << usrs[i].acct << " / "
              << usrs[i].nm << " [" << domain::ToString(usrs[i].st)
              << "] 角色:" << JoinRoles(ctx_->rls->RolesOf(usrs[i].id))
              << std::endl;
  }
}

void ConsoleView::CreateAccount(domain::Id uid) {
  const std::string acct = io::ReadLine("账号: ");
  const std::string nm = io::ReadLine("姓名: ");
  const std::string pwd = io::ReadLine("初始密码: ");
  io::PrintInfo("角色：1 普通参与者 / 2 组织者 / 3 检票员 / 4 管理员");
  const int ch = io::ReadInt("选择角色: ");
  domain::Role rl = domain::Role::kOrdinaryParticipant;
  if (!ParseRoleChoice(ch, &rl)) {
    io::PrintFail("无效角色");
    return;
  }
  domain::Id newUid = 0;
  const service::ServiceResult res =
      ctx_->auth->CreateAccount(uid, acct, nm, pwd, rl, &newUid);
  if (res.ok) {
    io::PrintOk(res.msg);
    io::PrintInfo("新账号 ID：" + std::to_string(newUid));
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::AssignRole(domain::Id uid) {
  ListUsers();
  const domain::Id tgtUid = io::ReadId("目标用户 ID: ");
  io::PrintInfo("角色：1 普通参与者 / 2 组织者 / 3 检票员 / 4 管理员");
  const int ch = io::ReadInt("要分配的角色: ");
  domain::Role rl = domain::Role::kOrdinaryParticipant;
  if (!ParseRoleChoice(ch, &rl)) {
    io::PrintFail("无效角色");
    return;
  }
  const service::ServiceResult res = ctx_->rls->AssignRole(uid, tgtUid, rl);
  if (res.ok) {
    io::PrintOk(res.msg);
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::ChangeUserStatus(domain::Id uid) {
  ListUsers();
  const domain::Id tgtUid = io::ReadId("目标用户 ID: ");
  const int ch = io::ReadInt("1 启用 / 2 停用: ");
  const domain::UserStatus st =
      ch == 1 ? domain::UserStatus::kActive : domain::UserStatus::kDisabled;
  const service::ServiceResult res = ctx_->auth->SetStatus(uid, tgtUid, st);
  if (res.ok) {
    io::PrintOk(res.msg);
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::GrantSession(domain::Id uid) {
  const std::vector<domain::LogicalSession> ssns =
      ctx_->ssns->AllLogicalSessions();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    std::cout << "  场次逻辑#" << ssns[i].id << " ["
              << (ssns[i].cncl ? "已取消" : "正常") << "]" << std::endl;
  }
  const domain::Id inspId = io::ReadId("检票员用户 ID: ");
  const domain::Id lsid = io::ReadId("场次逻辑 ID: ");
  const service::ServiceResult res = ctx_->rls->GrantSession(uid, inspId, lsid);
  if (res.ok) {
    io::PrintOk(res.msg);
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::ShowAuditLogs() {
  const std::vector<domain::AuditLog> lgs = ctx_->aud->Recent(20);
  if (lgs.empty()) {
    io::PrintInfo("暂无审计日志。");
    return;
  }
  for (std::size_t i = 0; i < lgs.size(); ++i) {
    std::cout << "  #" << lgs[i].id << " " << domain::FormatTime(lgs[i].crtTs)
              << " 用户" << lgs[i].uid << " " << lgs[i].actn << " ["
              << domain::ToString(lgs[i].res) << "] " << lgs[i].det
              << std::endl;
  }
}

void ConsoleView::BackgroundTasks(domain::Id uid) {
  while (true) {
    std::cout << "\n--- 后台任务 ---" << std::endl;
    std::cout << "1. 报名截止提醒" << std::endl;
    std::cout << "2. 缺勤判定" << std::endl;
    std::cout << "3. 失败通知重试" << std::endl;
    std::cout << "4. 查看任务列表" << std::endl;
    std::cout << "0. 返回" << std::endl;
    const int ch = io::ReadInt("请选择: ");
    if (ch == 0) {
      return;
    }
    if (ch == 1) {
      const int n = ctx_->bg->RunDeadlineNotification(uid);
      io::PrintInfo("已处理 " + std::to_string(n) + " 个截止提醒任务");
      continue;
    }
    if (ch == 2) {
      const int n = ctx_->bg->RunAbsenceDetection(uid);
      io::PrintInfo("判定缺勤 " + std::to_string(n) + " 人");
      continue;
    }
    if (ch == 3) {
      const int n = ctx_->bg->RetryFailedTasks(uid);
      io::PrintInfo("重试成功 " + std::to_string(n) + " 个任务");
      continue;
    }
    if (ch == 4) {
      ShowTasks();
      continue;
    }
    io::PrintFail("无效选项");
  }
}

void ConsoleView::ShowTasks() {
  const std::vector<domain::BackgroundTask> tsks = ctx_->bg->Tasks();
  if (tsks.empty()) {
    io::PrintInfo("暂无后台任务。");
    return;
  }
  for (std::size_t i = 0; i < tsks.size(); ++i) {
    std::cout << "  #" << tsks[i].id << " " << tsks[i].tskTyp
              << " payload=" << tsks[i].pl << " ["
              << domain::ToString(tsks[i].st) << "] 重试 " << tsks[i].retryN
              << " 次" << std::endl;
  }
}

}
}
