

#ifndef UI_CONSOLE_VIEW_H_
#define UI_CONSOLE_VIEW_H_

#include <vector>

#include "domain/enums.h"
#include "domain/types.h"

namespace campus {
namespace app {
struct AppContext;
}

namespace ui {

class ConsoleView {
 public:
  explicit ConsoleView(app::AppContext* ctx);

  void Run();

 private:
  void PrintBanner();
  void LoginLoop();
  bool EnsurePasswordChanged(domain::Id uid);
  domain::Role ChooseRole(const std::vector<domain::Role>& rls);
  void Dispatch(domain::Role rl, domain::Id uid);

  void ParticipantMenu(domain::Id uid);
  void OrganizerMenu(domain::Id uid);
  void InspectorMenu(domain::Id uid);
  void AdminMenu(domain::Id uid);

  void ShowPublishedActivities();
  void ShowActivityDetail(domain::Id aid);
  void DoRegister(domain::Id uid);
  void DoCancelRegistration(domain::Id uid);
  void ShowMyRegistrations(domain::Id uid);
  void ShowMyTickets(domain::Id uid);
  void ShowNotifications(domain::Id uid);

  void ShowOrganizerVersions(domain::Id uid);
  void CreateActivity(domain::Id uid);
  void EditDraft(domain::Id uid);
  void ManageSessions(domain::Id uid);
  void SubmitReview(domain::Id uid);
  void PublishVersion(domain::Id uid);
  void CancelActivity(domain::Id uid);
  void ShowStats(domain::Id uid);
  void ShowVersionDetail(domain::Id vid);

  void CheckIn(domain::Id uid);
  void ShowGrants(domain::Id uid);

  void ReviewVersions(domain::Id uid);
  void ListUsers();
  void CreateAccount(domain::Id uid);
  void AssignRole(domain::Id uid);
  void ChangeUserStatus(domain::Id uid);
  void GrantSession(domain::Id uid);
  void ShowAuditLogs();
  void BackgroundTasks(domain::Id uid);
  void ShowTasks();

  app::AppContext* ctx_;
};

}
}

#endif
