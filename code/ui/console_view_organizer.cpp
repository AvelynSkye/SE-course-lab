

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "app/app_context.h"
#include "domain/datetime.h"
#include "domain/entities.h"
#include "infra/data_store.h"
#include "service/activity_service.h"
#include "service/registration_service.h"
#include "service/role_service.h"
#include "service/service_result.h"
#include "service/session_service.h"
#include "service/stats_service.h"
#include "service/ticket_service.h"
#include "ui/console_io.h"
#include "ui/console_view.h"

namespace campus {
namespace ui {

void ConsoleView::OrganizerMenu(domain::Id uid) {
  while (true) {
    std::cout << "\n--- 组织者菜单 ---" << std::endl;
    std::cout << "1. 我的活动与版本" << std::endl;
    std::cout << "2. 创建活动" << std::endl;
    std::cout << "3. 编辑草稿 / 被驳回版本" << std::endl;
    std::cout << "4. 场次与类别维护" << std::endl;
    std::cout << "5. 提交审核" << std::endl;
    std::cout << "6. 发布活动（发布前复核）" << std::endl;
    std::cout << "7. 取消活动" << std::endl;
    std::cout << "8. 名单统计与导出" << std::endl;
    std::cout << "0. 退出登录" << std::endl;
    const int ch = io::ReadInt("请选择: ");
    switch (ch) {
      case 1:
        ShowOrganizerVersions(uid);
        break;
      case 2:
        CreateActivity(uid);
        break;
      case 3:
        EditDraft(uid);
        break;
      case 4:
        ManageSessions(uid);
        break;
      case 5:
        SubmitReview(uid);
        break;
      case 6:
        PublishVersion(uid);
        break;
      case 7:
        CancelActivity(uid);
        break;
      case 8:
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

void ConsoleView::ShowOrganizerVersions(domain::Id uid) {
  const std::vector<domain::Activity> acts = ctx_->acts->ActivitiesOf(uid);
  if (acts.empty()) {
    io::PrintInfo("您还没有创建活动。");
    return;
  }
  for (std::size_t i = 0; i < acts.size(); ++i) {
    std::cout << "\n[活动#" << acts[i].id << "] " << acts[i].ttl << " ["
              << domain::ToString(acts[i].st) << "]" << std::endl;
    const std::vector<domain::ActivityVersion> vers =
        ctx_->acts->VersionsOf(acts[i].id);
    for (std::size_t j = 0; j < vers.size(); ++j) {
      std::cout << "  版本#" << vers[j].id << " V" << vers[j].verNo << " ["
                << domain::ToString(vers[j].st) << "]";
      if (!vers[j].rej.empty()) {
        std::cout << " 驳回原因: " << vers[j].rej;
      }
      std::cout << std::endl;
      if (vers[j].st == domain::VersionStatus::kDraft ||
          vers[j].st == domain::VersionStatus::kReviewRejected ||
          vers[j].st == domain::VersionStatus::kApprovedPendingPublish ||
          vers[j].st == domain::VersionStatus::kCurrentPublished) {
        ShowVersionDetail(vers[j].id);
      }
    }
  }
}

void ConsoleView::CreateActivity(domain::Id uid) {
  const std::string ttl = io::ReadLine("活动标题: ");
  const std::string desc = io::ReadLine("活动简介: ");
  domain::Id aid = 0;
  const service::ServiceResult res =
      ctx_->acts->CreateActivity(uid, ttl, desc, &aid);
  if (!res.ok) {
    io::PrintFail(res.msg);
    return;
  }
  io::PrintOk(res.msg);
  domain::ActivityVersion* ver = ctx_->acts->LatestVersion(aid);
  if (ver != nullptr) {
    io::PrintInfo("当前草稿版本 ID：" + std::to_string(ver->id) +
                  "，请使用菜单 4 维护场次与类别");
  }
}

void ConsoleView::EditDraft(domain::Id uid) {
  ShowOrganizerVersions(uid);
  const domain::Id vid = io::ReadId("请输入要编辑的版本 ID: ");
  const domain::ActivityVersion* ver = ctx_->acts->FindVersion(vid);
  if (ver == nullptr) {
    io::PrintFail("版本不存在");
    return;
  }
  const domain::Activity* act = ctx_->acts->FindActivity(ver->aid);
  if (act == nullptr || act->crtId != uid) {
    io::PrintFail("只能编辑自己创建的活动");
    return;
  }
  const std::string ttl = io::ReadLine("新标题（当前：" + act->ttl + "）: ");
  const std::string desc = io::ReadLine("新简介: ");
  const service::ServiceResult res = ctx_->acts->EditDraft(uid, vid, ttl, desc);
  if (res.ok) {
    io::PrintOk(res.msg);
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::ManageSessions(domain::Id uid) {
  const domain::Id vid = io::ReadId("请输入要维护的版本 ID: ");
  if (ctx_->acts->FindVersion(vid) == nullptr) {
    io::PrintFail("版本不存在");
    return;
  }
  while (true) {
    std::cout << "\n--- 场次与类别维护（版本#" << vid << "）---" << std::endl;
    std::cout << "1. 查看场次与类别" << std::endl;
    std::cout << "2. 新增场次" << std::endl;
    std::cout << "3. 新增类别" << std::endl;
    std::cout << "4. 调整类别名额" << std::endl;
    std::cout << "5. 取消场次" << std::endl;
    std::cout << "6. 取消类别" << std::endl;
    std::cout << "0. 返回" << std::endl;
    const int ch = io::ReadInt("请选择: ");
    if (ch == 0) {
      return;
    }
    if (ch == 1) {
      ShowVersionDetail(vid);
      continue;
    }
    if (ch == 2) {
      const std::string loc = io::ReadLine("场次地点: ");
      const domain::Timestamp begTs =
          io::ReadTime("开始时间 (YYYY-MM-DD HH:MM): ");
      const domain::Timestamp endTs =
          io::ReadTime("结束时间 (YYYY-MM-DD HH:MM): ");
      const domain::Timestamp regBeg =
          io::ReadTime("报名开始时间 (YYYY-MM-DD HH:MM): ");
      const domain::Timestamp regEndTs =
          io::ReadTime("报名截止时间 (YYYY-MM-DD HH:MM): ");
      const int cap = io::ReadInt("场次容量: ");
      domain::Id lsid = 0;
      const service::ServiceResult res = ctx_->ssns->CreateSession(
          uid, vid, loc, begTs, endTs, regBeg, regEndTs, cap, &lsid);
      if (res.ok) {
        io::PrintOk(res.msg);
      } else {
        io::PrintFail(res.msg);
      }
      continue;
    }
    if (ch == 3) {
      const std::string nm = io::ReadLine("类别名称: ");
      const int cap = io::ReadInt("类别名额: ");
      domain::Id lcid = 0;
      const service::ServiceResult res =
          ctx_->ssns->CreateCategory(uid, vid, nm, cap, &lcid);
      if (res.ok) {
        io::PrintOk(res.msg);
      } else {
        io::PrintFail(res.msg);
      }
      continue;
    }
    if (ch == 4) {
      const domain::Id cvid = io::ReadId("类别版本 ID（非逻辑 ID）: ");
      const int cap = io::ReadInt("新的名额: ");
      const service::ServiceResult res =
          ctx_->ssns->UpdateCategoryQuota(uid, cvid, cap);
      if (res.ok) {
        io::PrintOk(res.msg);
      } else {
        io::PrintFail(res.msg);
      }
      continue;
    }
    if (ch == 5) {
      const domain::Id lsid = io::ReadId("要取消的场次逻辑 ID: ");
      if (!io::Confirm("确认取消该场次")) {
        continue;
      }
      const service::ServiceResult res = ctx_->ssns->CancelSession(uid, lsid);
      if (res.ok) {
        io::PrintOk(res.msg);
      } else {
        io::PrintFail(res.msg);
      }
      continue;
    }
    if (ch == 6) {
      const domain::Id lcid = io::ReadId("要取消的类别逻辑 ID: ");
      if (!io::Confirm("确认取消该类别")) {
        continue;
      }
      const service::ServiceResult res = ctx_->ssns->CancelCategory(uid, lcid);
      if (res.ok) {
        io::PrintOk(res.msg);
      } else {
        io::PrintFail(res.msg);
      }
      continue;
    }
    io::PrintFail("无效选项");
  }
}

void ConsoleView::SubmitReview(domain::Id uid) {
  ShowOrganizerVersions(uid);
  const domain::Id vid = io::ReadId("请输入要提交审核的版本 ID: ");
  const service::ServiceResult res = ctx_->acts->SubmitReview(uid, vid);
  if (res.ok) {
    io::PrintOk(res.msg);
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::PublishVersion(domain::Id uid) {
  ShowOrganizerVersions(uid);
  const domain::Id vid = io::ReadId("请输入要发布的版本 ID: ");
  std::string rsn;
  const service::ServiceResult checked =
      ctx_->acts->ValidateForPublish(vid, &rsn);
  io::PrintInfo("发布前复核结果：" + rsn);
  if (!checked.ok) {
    return;
  }
  if (!io::Confirm("确认发布该版本")) {
    return;
  }
  const service::ServiceResult res = ctx_->acts->Publish(uid, vid);
  if (res.ok) {
    io::PrintOk(res.msg);
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::CancelActivity(domain::Id uid) {
  ShowOrganizerVersions(uid);
  const domain::Id aid = io::ReadId("请输入要取消的活动 ID: ");
  if (!io::Confirm("确认取消该活动（将撤销全部报名）")) {
    return;
  }
  const service::ServiceResult res = ctx_->acts->CancelActivity(uid, aid);
  if (res.ok) {
    io::PrintOk(res.msg);
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::ShowStats(domain::Id uid) {
  const domain::Id aid = io::ReadId("请输入活动 ID: ");
  const std::vector<service::StatResult> sts = ctx_->sts->QueryStats(uid, aid);
  if (sts.empty()) {
    io::PrintFail("活动不存在或无权查看");
    return;
  }
  io::PrintInfo(sts[0].ToString());
  if (!io::Confirm("是否导出名单 CSV")) {
    return;
  }
  std::string pth =
      io::ReadLine("导出路径（回车使用默认 " + ctx_->ddir + "/roster.csv）: ");
  if (pth.empty()) {
    pth = ctx_->ddir + "/roster_" + std::to_string(aid) + ".csv";
  }
  std::string sum;
  const service::ServiceResult res =
      ctx_->sts->ExportRoster(uid, aid, pth, &sum);
  if (res.ok) {
    io::PrintOk(res.msg);
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::InspectorMenu(domain::Id uid) {
  while (true) {
    std::cout << "\n--- 检票员菜单 ---" << std::endl;
    std::cout << "1. 输入凭证码验票签到" << std::endl;
    std::cout << "2. 查看我的场次授权" << std::endl;
    std::cout << "0. 退出登录" << std::endl;
    const int ch = io::ReadInt("请选择: ");
    switch (ch) {
      case 1:
        CheckIn(uid);
        break;
      case 2:
        ShowGrants(uid);
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

void ConsoleView::CheckIn(domain::Id uid) {
  const std::string cd = io::ReadLine("请输入或扫描凭证码: ");
  const service::ServiceResult res = ctx_->tkts->CheckIn(uid, cd);
  if (res.ok) {
    io::PrintOk(res.msg);
  } else {
    io::PrintFail(res.msg);
  }
}

void ConsoleView::ShowGrants(domain::Id uid) {
  const std::vector<domain::SessionGrant> grts = ctx_->rls->GrantsOf(uid);
  if (grts.empty()) {
    io::PrintInfo("暂无场次授权。");
    return;
  }
  for (std::size_t i = 0; i < grts.size(); ++i) {
    std::cout << "  场次逻辑#" << grts[i].lsid << " ["
              << domain::ToString(grts[i].st) << "]" << std::endl;
  }
}

}
}
