

#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>
#include <string>

#include "app/app_context.h"
#include "app/seed.h"
#include "infra/data_store.h"
#include "service/activity_service.h"
#include "service/audit_service.h"
#include "service/auth_service.h"
#include "service/background_task_service.h"
#include "service/notification_service.h"
#include "service/registration_service.h"
#include "service/role_service.h"
#include "service/session_service.h"
#include "service/stats_service.h"
#include "service/ticket_service.h"
#include "ui/console_io.h"
#include "ui/console_view.h"

int main() {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#endif
  campus::infra::DataStore db;
  campus::app::AppContext ctx;
  ctx.ddir = "data";
  if (!db.Load(ctx.ddir)) {
    std::cerr << "数据加载失败，请检查缺失或损坏的文件；启动已停止。"
              << std::endl;
    return 1;
  }

  campus::service::AuditService aud(&db);
  campus::service::RoleService rls(&db, &aud);
  campus::service::NotificationService ntfs(&db, &aud, &rls);
  campus::service::AuthService auth(&db, &aud, &rls);
  campus::service::TicketService tkts(&db, &rls, &aud);
  campus::service::SessionService ssns(&db, &rls, &aud, &ntfs, &tkts);
  campus::service::ActivityService acts(&db, &rls, &aud, &ntfs, &tkts);
  campus::service::RegistrationService regs(&db, &rls, &aud, &ntfs, &tkts);
  campus::service::StatsService sts(&db, &rls, &aud);
  campus::service::BackgroundTaskService bg(&db, &aud, &ntfs, &rls, &tkts);

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
  ctx.bg = &bg;

  campus::app::SeedDemoData(&ctx);
  campus::ui::io::SetTick([&] {
    bg.RunDue();
    if (!db.Save(ctx.ddir))
      campus::ui::io::PrintFail("数据保存失败，请检查数据目录");
  });

  campus::ui::ConsoleView view(&ctx);
  view.Run();
  db.Save(ctx.ddir);
  std::cout << "演示账号：admin / organizer / inspector / student1 / "
            << "student2，初始密码均为 123456" << std::endl;
  return 0;
}
