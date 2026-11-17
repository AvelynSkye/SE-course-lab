

#include "service/background_task_service.h"

#include <cstddef>
#include <string>
#include <vector>

#include "domain/datetime.h"
#include "infra/data_store.h"
#include "service/audit_service.h"
#include "service/notification_service.h"

namespace campus {
namespace service {

namespace {

bool IsCurrentSession(infra::DataStore* db, const domain::Session& ssn) {
  std::vector<domain::ActivityVersion>& vers = db->activity_versions();
  for (std::size_t i = 0; i < vers.size(); ++i) {
    if (vers[i].id == ssn.vid) {
      return vers[i].st == domain::VersionStatus::kCurrentPublished;
    }
  }
  return false;
}

}

BackgroundTaskService::BackgroundTaskService(infra::DataStore* db,
                                             AuditService* aud,
                                             NotificationService* ntfs)
    : db_(db), aud_(aud), ntfs_(ntfs) {}

bool BackgroundTaskService::TaskExists(const std::string& tskTyp,
                                       const std::string& pl) {
  std::vector<domain::BackgroundTask>& tsks = db_->background_tasks();
  for (std::size_t i = 0; i < tsks.size(); ++i) {
    if (tsks[i].tskTyp == tskTyp && tsks[i].pl == pl) {
      return true;
    }
  }
  return false;
}

void BackgroundTaskService::Enqueue(const std::string& tskTyp,
                                    const std::string& pl,
                                    domain::TaskStatus st) {
  domain::BackgroundTask tsk;
  tsk.id = db_->NextId("background_task");
  tsk.tskTyp = tskTyp;
  tsk.pl = pl;
  tsk.st = st;
  tsk.retryN = 0;
  db_->background_tasks().push_back(tsk);
}

int BackgroundTaskService::RunDeadlineNotification(domain::Id opId) {
  int doneN = 0;
  const domain::Timestamp ts = domain::Now();
  std::vector<domain::Session>& ssns = db_->ssns();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (!IsCurrentSession(db_, ssns[i]) || ssns[i].regEndTs >= ts) {
      continue;
    }
    const std::string pl = std::to_string(ssns[i].lsid);
    if (TaskExists("deadline", pl)) {
      continue;
    }
    std::vector<domain::Registration>& regs = db_->regs();
    for (std::size_t j = 0; j < regs.size(); ++j) {
      const domain::Registration& reg = regs[j];
      if (reg.lsid != ssns[i].lsid) {
        continue;
      }
      if (reg.st == domain::RegistrationStatus::kRegistered ||
          reg.st == domain::RegistrationStatus::kWaitlisted) {
        ntfs_->Notify(reg.uid, domain::NotificationType::kRegistrationDeadline,
                      "场次 " + ssns[i].loc + " 报名已截止，请留意后续通知");
      }
    }
    Enqueue("deadline", pl, domain::TaskStatus::kSuccess);
    aud_->Log(opId, "截止提醒任务", domain::AuditResult::kSuccess,
              "场次 " + pl);
    ++doneN;
  }
  return doneN;
}

int BackgroundTaskService::RunAbsenceDetection(domain::Id opId) {
  int detN = 0;
  const domain::Timestamp ts = domain::Now();
  std::vector<domain::Session>& ssns = db_->ssns();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (!IsCurrentSession(db_, ssns[i]) || ssns[i].endTs >= ts) {
      continue;
    }
    std::vector<domain::Registration>& regs = db_->regs();
    for (std::size_t j = 0; j < regs.size(); ++j) {
      domain::Registration& reg = regs[j];
      if (reg.lsid != ssns[i].lsid || reg.vid != ssns[i].vid) {
        continue;
      }
      if (reg.st != domain::RegistrationStatus::kRegistered) {
        continue;
      }
      bool chkN = false;
      std::vector<domain::CheckinEvent>& evs = db_->checkin_events();
      for (std::size_t k = 0; k < evs.size(); ++k) {
        if (evs[k].rid == reg.id) {
          chkN = true;
          break;
        }
      }
      if (chkN) {
        continue;
      }
      const std::string pl = std::to_string(reg.id);
      if (TaskExists("absence", pl)) {
        continue;
      }
      Enqueue("absence", pl, domain::TaskStatus::kSuccess);
      aud_->Log(opId, "缺勤判定", domain::AuditResult::kSuccess,
                "报名 " + pl + " 判定缺勤");
      ++detN;
    }
  }
  return detN;
}

int BackgroundTaskService::RetryFailedTasks(domain::Id opId) {
  int retryN = 0;
  std::vector<domain::BackgroundTask>& tsks = db_->background_tasks();
  for (std::size_t i = 0; i < tsks.size(); ++i) {
    if (tsks[i].st != domain::TaskStatus::kFailed) {
      continue;
    }
    if (tsks[i].retryN >= 3) {
      continue;
    }
    ++tsks[i].retryN;
    tsks[i].st = domain::TaskStatus::kSuccess;
    aud_->Log(opId, "通知重试", domain::AuditResult::kSuccess,
              "任务 " + std::to_string(tsks[i].id) + " 第 " +
                  std::to_string(tsks[i].retryN) + " 次重试成功");
    ++retryN;
  }
  return retryN;
}

std::vector<domain::BackgroundTask> BackgroundTaskService::Tasks() {
  return db_->background_tasks();
}

}
}
