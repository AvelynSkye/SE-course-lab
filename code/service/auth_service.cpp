

#include "service/auth_service.h"

#include <cstddef>
#include <string>
#include <vector>

#include "domain/datetime.h"
#include "infra/data_store.h"
#include "infra/security.h"
#include "service/audit_service.h"
#include "service/role_service.h"

namespace campus {
namespace service {

AuthService::AuthService(infra::DataStore* db, AuditService* aud,
                         RoleService* rls)
    : db_(db), aud_(aud), rls_(rls) {}

bool AuthService::IsLocked(const domain::User& usr) {
  return usr.lockTs > domain::Now();
}

ServiceResult AuthService::Login(const std::string& acct,
                                 const std::string& pwd, domain::Id* uid) {
  domain::User* usr = FindByAccount(acct);
  if (usr == nullptr) {
    aud_->Log(0, "登录", domain::AuditResult::kFailure, "账号不存在：" + acct);
    return ServiceResult::Fail("账号不存在");
  }
  if (usr->st == domain::UserStatus::kDisabled) {
    aud_->Log(usr->id, "登录", domain::AuditResult::kFailure, "账号已停用");
    return ServiceResult::Fail("账号已停用，请联系管理员");
  }
  if (IsLocked(*usr)) {
    aud_->Log(usr->id, "登录", domain::AuditResult::kFailure, "账号锁定中");
    return ServiceResult::Fail("账号已锁定，请于 " +
                               domain::FormatTime(usr->lockTs) + " 后重试");
  }
  if (!infra::PasswordHasher::Verify(acct, pwd, usr->pwdH)) {
    ++usr->failN;
    if (usr->failN >= kMaxFailCount) {
      usr->lockTs = domain::AddMinutes(domain::Now(), kLockMinutes);
      usr->failN = 0;
      aud_->Log(usr->id, "登录", domain::AuditResult::kFailure,
                "连续失败触发锁定");
      return ServiceResult::Fail("连续登录失败，账号锁定 " +
                                 std::to_string(kLockMinutes) + " 分钟");
    }
    const int remN = kMaxFailCount - usr->failN;
    aud_->Log(usr->id, "登录", domain::AuditResult::kFailure, "密码错误");
    return ServiceResult::Fail("密码错误，剩余 " + std::to_string(remN) +
                               " 次尝试");
  }
  usr->failN = 0;
  usr->lockTs = 0;
  *uid = usr->id;
  aud_->Log(usr->id, "登录", domain::AuditResult::kSuccess, acct);
  if (usr->forcePwd) {
    return ServiceResult::Ok("登录成功，首次登录需修改初始密码");
  }
  return ServiceResult::Ok("登录成功，欢迎 " + usr->nm);
}

ServiceResult AuthService::ChangePassword(domain::Id uid,
                                          const std::string& oldPwd,
                                          const std::string& newPwd) {
  domain::User* usr = FindUser(uid);
  if (usr == nullptr) {
    return ServiceResult::Fail("用户不存在");
  }
  if (!infra::PasswordHasher::Verify(usr->acct, oldPwd, usr->pwdH)) {
    aud_->Log(uid, "修改密码", domain::AuditResult::kFailure, "原密码错误");
    return ServiceResult::Fail("原密码错误");
  }
  if (newPwd.size() < static_cast<std::size_t>(kMinPasswordLength)) {
    return ServiceResult::Fail("新密码长度不能少于 " +
                               std::to_string(kMinPasswordLength) + " 位");
  }
  if (newPwd == oldPwd) {
    return ServiceResult::Fail("新密码不能与原密码相同");
  }
  usr->pwdH = infra::PasswordHasher::Hash(usr->acct, newPwd);
  usr->forcePwd = false;
  aud_->Log(uid, "修改密码", domain::AuditResult::kSuccess, "密码已更新");
  return ServiceResult::Ok("密码修改成功");
}

ServiceResult AuthService::CreateAccount(domain::Id opId,
                                         const std::string& acct,
                                         const std::string& nm,
                                         const std::string& pwd,
                                         domain::Role rl, domain::Id* uid) {
  if (opId != 0 && !rls_->HasRole(opId, domain::Role::kAdministrator)) {
    return ServiceResult::Fail("仅管理员可以创建账号");
  }
  if (acct.empty() || nm.empty()) {
    return ServiceResult::Fail("账号与姓名不能为空");
  }
  if (FindByAccount(acct) != nullptr) {
    return ServiceResult::Fail("账号已存在");
  }
  if (pwd.size() < static_cast<std::size_t>(kMinPasswordLength)) {
    return ServiceResult::Fail("初始密码长度不能少于 6 位");
  }
  domain::User usr;
  usr.id = db_->NextId("user");
  usr.acct = acct;
  usr.nm = nm;
  usr.pwdH = infra::PasswordHasher::Hash(acct, pwd);
  usr.st = domain::UserStatus::kActive;
  usr.forcePwd = true;
  usr.failN = 0;
  usr.lockTs = 0;
  db_->usrs().push_back(usr);

  domain::UserRole usrRl;
  usrRl.uid = usr.id;
  usrRl.rlCd = rl;
  db_->user_roles().push_back(usrRl);

  *uid = usr.id;
  aud_->Log(opId, "创建账号", domain::AuditResult::kSuccess,
            acct + " / " + domain::ToString(rl));
  return ServiceResult::Ok("账号创建成功，初始密码需首次登录后修改");
}

ServiceResult AuthService::SetStatus(domain::Id opId, domain::Id uid,
                                     domain::UserStatus st) {
  if (!rls_->HasRole(opId, domain::Role::kAdministrator)) {
    return ServiceResult::Fail("仅管理员可以修改账号状态");
  }
  if (opId == uid) {
    return ServiceResult::Fail("不能修改自己的账号状态");
  }
  domain::User* usr = FindUser(uid);
  if (usr == nullptr) {
    return ServiceResult::Fail("用户不存在");
  }
  usr->st = st;
  aud_->Log(opId, "修改账号状态", domain::AuditResult::kSuccess,
            usr->acct + " -> " + domain::ToString(st));
  return ServiceResult::Ok("账号状态已更新为" + domain::ToString(st));
}

domain::User* AuthService::FindUser(domain::Id uid) {
  std::vector<domain::User>& usrs = db_->usrs();
  for (std::size_t i = 0; i < usrs.size(); ++i) {
    if (usrs[i].id == uid) {
      return &usrs[i];
    }
  }
  return nullptr;
}

domain::User* AuthService::FindByAccount(const std::string& acct) {
  std::vector<domain::User>& usrs = db_->usrs();
  for (std::size_t i = 0; i < usrs.size(); ++i) {
    if (usrs[i].acct == acct) {
      return &usrs[i];
    }
  }
  return nullptr;
}

std::vector<domain::User> AuthService::AllUsers() { return db_->usrs(); }

}
}
