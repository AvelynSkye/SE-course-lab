

#include "service/role_service.h"

#include <cstddef>
#include <string>
#include <vector>

#include "infra/data_store.h"
#include "service/audit_service.h"

namespace campus {
namespace service {

namespace {

bool IsAdministrator(domain::Role rl) {
  return rl == domain::Role::kAdministrator;
}

}

RoleService::RoleService(infra::DataStore* db, AuditService* aud)
    : db_(db), aud_(aud) {}

std::vector<domain::Role> RoleService::RolesOf(domain::Id uid) {
  std::vector<domain::Role> rls;
  std::vector<domain::UserRole>& xsAll = db_->user_roles();
  for (std::size_t i = 0; i < xsAll.size(); ++i) {
    if (xsAll[i].uid == uid) {
      rls.push_back(xsAll[i].rlCd);
    }
  }
  return rls;
}

bool RoleService::HasRole(domain::Id uid, domain::Role rl) {
  const std::vector<domain::Role> rls = RolesOf(uid);
  for (std::size_t i = 0; i < rls.size(); ++i) {
    if (rls[i] == rl) {
      return true;
    }
  }
  return false;
}

bool RoleService::CanAccess(domain::Role rl, const std::string& modCd) {
  if (IsAdministrator(rl)) {
    return true;
  }
  if (rl == domain::Role::kOrdinaryParticipant) {
    return modCd == "M5" || modCd == "M6" || modCd == "M8";
  }
  if (rl == domain::Role::kOrganizer) {
    return modCd == "M3" || modCd == "M4" || modCd == "M7" || modCd == "M8";
  }
  if (rl == domain::Role::kInspector) {
    return modCd == "M6" || modCd == "M8";
  }
  return false;
}

ServiceResult RoleService::AssignRole(domain::Id opId, domain::Id tgtUid,
                                      domain::Role rl) {
  if (!HasRole(opId, domain::Role::kAdministrator)) {
    aud_->Log(opId, "分配角色", domain::AuditResult::kFailure, "无管理员权限");
    return ServiceResult::Fail("仅管理员可以分配角色");
  }
  if (!UserExists(tgtUid)) {
    return ServiceResult::Fail("目标用户不存在");
  }
  if (HasRole(tgtUid, rl)) {
    return ServiceResult::Fail("该用户已拥有此角色");
  }
  domain::UserRole usrRl;
  usrRl.uid = tgtUid;
  usrRl.rlCd = rl;
  db_->user_roles().push_back(usrRl);
  aud_->Log(
      opId, "分配角色", domain::AuditResult::kSuccess,
      "用户 " + std::to_string(tgtUid) + " 获得角色 " + domain::ToString(rl));
  return ServiceResult::Ok("角色分配成功");
}

ServiceResult RoleService::RemoveRole(domain::Id opId, domain::Id tgtUid,
                                      domain::Role rl) {
  if (!HasRole(opId, domain::Role::kAdministrator)) {
    return ServiceResult::Fail("仅管理员可以移除角色");
  }
  const std::vector<domain::Role> rls = RolesOf(tgtUid);
  if (rls.size() <= 1) {
    return ServiceResult::Fail("不能移除用户的最后一个角色");
  }
  std::vector<domain::UserRole>& xsAll = db_->user_roles();
  for (std::size_t i = 0; i < xsAll.size(); ++i) {
    if (xsAll[i].uid == tgtUid && xsAll[i].rlCd == rl) {
      xsAll.erase(xsAll.begin() + static_cast<std::ptrdiff_t>(i));
      aud_->Log(opId, "移除角色", domain::AuditResult::kSuccess,
                "用户 " + std::to_string(tgtUid) + " 移除角色 " +
                    domain::ToString(rl));
      return ServiceResult::Ok("角色已移除");
    }
  }
  return ServiceResult::Fail("该用户不拥有此角色");
}

ServiceResult RoleService::GrantSession(domain::Id opId, domain::Id inspId,
                                        domain::Id lsid) {
  if (!HasRole(opId, domain::Role::kAdministrator)) {
    return ServiceResult::Fail("仅管理员可以授予场次权限");
  }
  if (!HasRole(inspId, domain::Role::kInspector)) {
    return ServiceResult::Fail("目标用户不是检票员");
  }
  bool ssnOk = false;
  std::vector<domain::LogicalSession>& ssns = db_->logical_sessions();
  for (std::size_t i = 0; i < ssns.size(); ++i) {
    if (ssns[i].id == lsid) {
      ssnOk = true;
      break;
    }
  }
  if (!ssnOk) {
    return ServiceResult::Fail("逻辑场次不存在");
  }
  std::vector<domain::SessionGrant>& grts = db_->session_grants();
  for (std::size_t i = 0; i < grts.size(); ++i) {
    if (grts[i].uid == inspId && grts[i].lsid == lsid) {
      grts[i].st = domain::GrantStatus::kValid;
      aud_->Log(
          opId, "场次授权", domain::AuditResult::kSuccess,
          "检票员 " + std::to_string(inspId) + " 场次 " + std::to_string(lsid));
      return ServiceResult::Ok("场次授权已更新");
    }
  }
  domain::SessionGrant grt;
  grt.uid = inspId;
  grt.lsid = lsid;
  grt.st = domain::GrantStatus::kValid;
  grts.push_back(grt);
  aud_->Log(
      opId, "场次授权", domain::AuditResult::kSuccess,
      "检票员 " + std::to_string(inspId) + " 场次 " + std::to_string(lsid));
  return ServiceResult::Ok("场次授权成功");
}

std::vector<domain::SessionGrant> RoleService::GrantsOf(domain::Id uid) {
  std::vector<domain::SessionGrant> res;
  std::vector<domain::SessionGrant>& grts = db_->session_grants();
  for (std::size_t i = 0; i < grts.size(); ++i) {
    if (grts[i].uid == uid) {
      res.push_back(grts[i]);
    }
  }
  return res;
}

bool RoleService::HasValidGrant(domain::Id inspId, domain::Id lsid) {
  std::vector<domain::SessionGrant>& grts = db_->session_grants();
  for (std::size_t i = 0; i < grts.size(); ++i) {
    if (grts[i].uid == inspId && grts[i].lsid == lsid &&
        grts[i].st == domain::GrantStatus::kValid) {
      return true;
    }
  }
  return false;
}

bool RoleService::UserExists(domain::Id uid) {
  std::vector<domain::User>& usrs = db_->usrs();
  for (std::size_t i = 0; i < usrs.size(); ++i) {
    if (usrs[i].id == uid) {
      return true;
    }
  }
  return false;
}

}
}
