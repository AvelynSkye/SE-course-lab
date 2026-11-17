

#ifndef DOMAIN_ENUMS_H_
#define DOMAIN_ENUMS_H_

#include <string>

namespace campus {
namespace domain {

enum class Role {
  kOrdinaryParticipant = 0,
  kOrganizer,
  kInspector,
  kAdministrator
};

enum class UserStatus { kActive = 0, kDisabled };

enum class ActivityStatus { kNotPublished = 0, kPublished, kCancelled, kEnded };

enum class VersionStatus {
  kDraft = 0,
  kPendingReview,
  kReviewRejected,
  kApprovedPendingPublish,
  kCurrentPublished,
  kHistory
};

enum class GrantStatus { kValid = 0, kInvalid };

enum class RegistrationStatus {
  kWaitlisted = 0,
  kRegistered,
  kCheckedIn,
  kCancelled
};

enum class TicketStatus { kValid = 0, kInvalid };

enum class AuditResult { kSuccess = 0, kFailure };

enum class NotificationType {
  kPromoted = 0,
  kRegistrationDeadline,
  kActivityCancelled,
  kSessionCancelled,
  kCategoryCancelled
};

enum class TaskStatus { kPending = 0, kSuccess, kFailed };

std::string ToString(Role val);
std::string ToString(UserStatus val);
std::string ToString(ActivityStatus val);
std::string ToString(VersionStatus val);
std::string ToString(GrantStatus val);
std::string ToString(RegistrationStatus val);
std::string ToString(TicketStatus val);
std::string ToString(AuditResult val);
std::string ToString(NotificationType val);
std::string ToString(TaskStatus val);

}
}

#endif
