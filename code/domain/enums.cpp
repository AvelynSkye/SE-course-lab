

#include "domain/enums.h"

#include <string>

namespace campus {
namespace domain {

std::string ToString(Role val) {
  switch (val) {
    case Role::kOrdinaryParticipant:
      return "普通参与者";
    case Role::kOrganizer:
      return "组织者";
    case Role::kInspector:
      return "检票员";
    case Role::kAdministrator:
      return "管理员";
  }
  return "未知角色";
}

std::string ToString(UserStatus val) {
  switch (val) {
    case UserStatus::kActive:
      return "启用";
    case UserStatus::kDisabled:
      return "停用";
  }
  return "未知状态";
}

std::string ToString(ActivityStatus val) {
  switch (val) {
    case ActivityStatus::kNotPublished:
      return "未发布";
    case ActivityStatus::kPublished:
      return "已发布";
    case ActivityStatus::kCancelled:
      return "已取消";
    case ActivityStatus::kEnded:
      return "已结束";
  }
  return "未知状态";
}

std::string ToString(VersionStatus val) {
  switch (val) {
    case VersionStatus::kDraft:
      return "草稿";
    case VersionStatus::kPendingReview:
      return "待审核";
    case VersionStatus::kReviewRejected:
      return "审核驳回";
    case VersionStatus::kApprovedPendingPublish:
      return "审核通过待发布";
    case VersionStatus::kCurrentPublished:
      return "当前发布";
    case VersionStatus::kHistory:
      return "历史版本";
  }
  return "未知状态";
}

std::string ToString(GrantStatus val) {
  switch (val) {
    case GrantStatus::kValid:
      return "有效";
    case GrantStatus::kInvalid:
      return "失效";
  }
  return "未知状态";
}

std::string ToString(RegistrationStatus val) {
  switch (val) {
    case RegistrationStatus::kWaitlisted:
      return "候补";
    case RegistrationStatus::kRegistered:
      return "已报名";
    case RegistrationStatus::kCheckedIn:
      return "已签到";
    case RegistrationStatus::kCancelled:
      return "已取消";
  }
  return "未知状态";
}

std::string ToString(TicketStatus val) {
  switch (val) {
    case TicketStatus::kValid:
      return "有效";
    case TicketStatus::kInvalid:
      return "失效";
  }
  return "未知状态";
}

std::string ToString(AuditResult val) {
  switch (val) {
    case AuditResult::kSuccess:
      return "成功";
    case AuditResult::kFailure:
      return "失败";
  }
  return "未知结果";
}

std::string ToString(NotificationType val) {
  switch (val) {
    case NotificationType::kPromoted:
      return "递补";
    case NotificationType::kRegistrationDeadline:
      return "截止提醒";
    case NotificationType::kActivityCancelled:
      return "活动取消";
    case NotificationType::kSessionCancelled:
      return "场次取消";
    case NotificationType::kCategoryCancelled:
      return "类别取消";
  }
  return "未知类型";
}

std::string ToString(TaskStatus val) {
  switch (val) {
    case TaskStatus::kPending:
      return "待执行";
    case TaskStatus::kSuccess:
      return "成功";
    case TaskStatus::kFailed:
      return "失败";
  }
  return "未知状态";
}

}
}
