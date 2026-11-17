

#include "infra/data_store.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "domain/types.h"
#include "infra/csv_util.h"

namespace campus {
namespace infra {

namespace {

typedef std::vector<std::string> Row;
typedef std::function<void(const Row&)> RowHandler;

std::string Text(domain::Id val) { return std::to_string(val); }

std::string Text(int val) { return std::to_string(val); }

std::string Text(bool val) { return val ? "1" : "0"; }

std::string TextTime(domain::Timestamp val) {
  return std::to_string(static_cast<domain::Id>(val));
}

template <typename EnumType>
std::string TextEnum(EnumType val) {
  return std::to_string(static_cast<int>(val));
}

std::string MakeRow(const std::vector<std::string>& flds) {
  return JoinCsv(flds);
}

void ReadTable(const std::string& pth, const RowHandler& fn) {
  std::ifstream in(pth.c_str());
  if (!in.is_open()) {
    return;
  }
  std::string ln;
  while (std::getline(in, ln)) {
    if (ln.empty()) {
      continue;
    }
    fn(SplitCsv(ln));
  }
}

template <typename T>
void WriteTable(const std::string& pth, const std::vector<T>& xs,
                std::string (*ser)(const T&)) {
  std::ofstream out(pth.c_str());
  if (!out.is_open()) {
    return;
  }
  for (std::size_t i = 0; i < xs.size(); ++i) {
    out << ser(xs[i]) << "\n";
  }
}

domain::Timestamp ToTimestamp(const std::string& txt) {
  domain::Id val = 0;
  if (!ParseId(txt, &val)) {
    return 0;
  }
  return static_cast<domain::Timestamp>(val);
}

std::string RowOf(const domain::User& usr) {
  return MakeRow({Text(usr.id), usr.acct, usr.nm, usr.pwdH, TextEnum(usr.st),
                  Text(usr.forcePwd), Text(usr.failN), TextTime(usr.lockTs)});
}

void FillUser(const Row& x, domain::User* usr) {
  if (x.size() < 8) {
    return;
  }
  ParseId(x[0], &usr->id);
  usr->acct = x[1];
  usr->nm = x[2];
  usr->pwdH = x[3];
  int st = 0;
  if (ParseIntValue(x[4], &st)) {
    usr->st = static_cast<domain::UserStatus>(st);
  }
  ParseBoolValue(x[5], &usr->forcePwd);
  ParseIntValue(x[6], &usr->failN);
  usr->lockTs = ToTimestamp(x[7]);
}

std::string RowOf(const domain::UserRole& usrRl) {
  return MakeRow({Text(usrRl.uid), TextEnum(usrRl.rlCd)});
}

void FillUserRole(const Row& x, domain::UserRole* usrRl) {
  if (x.size() < 2) {
    return;
  }
  ParseId(x[0], &usrRl->uid);
  int rl = 0;
  if (ParseIntValue(x[1], &rl)) {
    usrRl->rlCd = static_cast<domain::Role>(rl);
  }
}

std::string RowOf(const domain::Activity& act) {
  return MakeRow({Text(act.id), act.ttl, TextEnum(act.st), Text(act.crtId)});
}

void FillActivity(const Row& x, domain::Activity* act) {
  if (x.size() < 4) {
    return;
  }
  ParseId(x[0], &act->id);
  act->ttl = x[1];
  int st = 0;
  if (ParseIntValue(x[2], &st)) {
    act->st = static_cast<domain::ActivityStatus>(st);
  }
  ParseId(x[3], &act->crtId);
}

std::string RowOf(const domain::ActivityVersion& ver) {
  return MakeRow({Text(ver.id), Text(ver.aid), Text(ver.verNo),
                  TextEnum(ver.st), TextTime(ver.pubAt), ver.desc, ver.rej});
}

void FillActivityVersion(const Row& x, domain::ActivityVersion* ver) {
  if (x.size() < 7) {
    return;
  }
  ParseId(x[0], &ver->id);
  ParseId(x[1], &ver->aid);
  ParseIntValue(x[2], &ver->verNo);
  int st = 0;
  if (ParseIntValue(x[3], &st)) {
    ver->st = static_cast<domain::VersionStatus>(st);
  }
  ver->pubAt = ToTimestamp(x[4]);
  ver->desc = x[5];
  ver->rej = x[6];
}

std::string RowOf(const domain::LogicalSession& lSsn) {
  return MakeRow({Text(lSsn.id), Text(lSsn.cncl)});
}

void FillLogicalSession(const Row& x, domain::LogicalSession* ssn) {
  if (x.size() < 2) {
    return;
  }
  ParseId(x[0], &ssn->id);
  ParseBoolValue(x[1], &ssn->cncl);
}

std::string RowOf(const domain::Session& ssn) {
  return MakeRow({Text(ssn.id), Text(ssn.lsid), Text(ssn.vid), ssn.loc,
                  TextTime(ssn.begTs), TextTime(ssn.endTs),
                  TextTime(ssn.regBeg), TextTime(ssn.regEndTs), Text(ssn.cap)});
}

void FillSession(const Row& x, domain::Session* ssn) {
  if (x.size() < 9) {
    return;
  }
  ParseId(x[0], &ssn->id);
  ParseId(x[1], &ssn->lsid);
  ParseId(x[2], &ssn->vid);
  ssn->loc = x[3];
  ssn->begTs = ToTimestamp(x[4]);
  ssn->endTs = ToTimestamp(x[5]);
  ssn->regBeg = ToTimestamp(x[6]);
  ssn->regEndTs = ToTimestamp(x[7]);
  ParseIntValue(x[8], &ssn->cap);
}

std::string RowOf(const domain::LogicalCategory& cat) {
  return MakeRow({Text(cat.id), Text(cat.cncl)});
}

void FillLogicalCategory(const Row& x, domain::LogicalCategory* cat) {
  if (x.size() < 2) {
    return;
  }
  ParseId(x[0], &cat->id);
  ParseBoolValue(x[1], &cat->cncl);
}

std::string RowOf(const domain::CategoryVersion& cat) {
  return MakeRow(
      {Text(cat.id), Text(cat.lcid), Text(cat.vid), cat.nm, Text(cat.cap)});
}

void FillCategoryVersion(const Row& x, domain::CategoryVersion* cat) {
  if (x.size() < 5) {
    return;
  }
  ParseId(x[0], &cat->id);
  ParseId(x[1], &cat->lcid);
  ParseId(x[2], &cat->vid);
  cat->nm = x[3];
  ParseIntValue(x[4], &cat->cap);
}

std::string RowOf(const domain::SessionGrant& grt) {
  return MakeRow({Text(grt.uid), Text(grt.lsid), TextEnum(grt.st)});
}

void FillSessionGrant(const Row& x, domain::SessionGrant* grt) {
  if (x.size() < 3) {
    return;
  }
  ParseId(x[0], &grt->uid);
  ParseId(x[1], &grt->lsid);
  int st = 0;
  if (ParseIntValue(x[2], &st)) {
    grt->st = static_cast<domain::GrantStatus>(st);
  }
}

std::string RowOf(const domain::Registration& reg) {
  return MakeRow({Text(reg.id), Text(reg.uid), Text(reg.lsid), Text(reg.lcid),
                  Text(reg.vid), TextEnum(reg.st), Text(reg.wlSeq),
                  TextTime(reg.crtTs), TextTime(reg.cnclTs)});
}

void FillRegistration(const Row& x, domain::Registration* reg) {
  if (x.size() < 9) {
    return;
  }
  ParseId(x[0], &reg->id);
  ParseId(x[1], &reg->uid);
  ParseId(x[2], &reg->lsid);
  ParseId(x[3], &reg->lcid);
  ParseId(x[4], &reg->vid);
  int st = 0;
  if (ParseIntValue(x[5], &st)) {
    reg->st = static_cast<domain::RegistrationStatus>(st);
  }
  ParseId(x[6], &reg->wlSeq);
  reg->crtTs = ToTimestamp(x[7]);
  reg->cnclTs = ToTimestamp(x[8]);
}

std::string RowOf(const domain::Ticket& tkt) {
  return MakeRow({Text(tkt.id), Text(tkt.rid), tkt.cd, TextEnum(tkt.st),
                  TextTime(tkt.issTs)});
}

void FillTicket(const Row& x, domain::Ticket* tkt) {
  if (x.size() < 5) {
    return;
  }
  ParseId(x[0], &tkt->id);
  ParseId(x[1], &tkt->rid);
  tkt->cd = x[2];
  int st = 0;
  if (ParseIntValue(x[3], &st)) {
    tkt->st = static_cast<domain::TicketStatus>(st);
  }
  tkt->issTs = ToTimestamp(x[4]);
}

std::string RowOf(const domain::CheckinEvent& ev) {
  return MakeRow({Text(ev.id), Text(ev.rid), TextTime(ev.chkTs)});
}

void FillCheckinEvent(const Row& x, domain::CheckinEvent* ev) {
  if (x.size() < 3) {
    return;
  }
  ParseId(x[0], &ev->id);
  ParseId(x[1], &ev->rid);
  ev->chkTs = ToTimestamp(x[2]);
}

std::string RowOf(const domain::Notification& ntf) {
  return MakeRow({Text(ntf.id), Text(ntf.uid), TextEnum(ntf.typ), ntf.cnt,
                  Text(ntf.rd), TextTime(ntf.crtTs)});
}

void FillNotification(const Row& x, domain::Notification* ntf) {
  if (x.size() < 6) {
    return;
  }
  ParseId(x[0], &ntf->id);
  ParseId(x[1], &ntf->uid);
  int typ = 0;
  if (ParseIntValue(x[2], &typ)) {
    ntf->typ = static_cast<domain::NotificationType>(typ);
  }
  ntf->cnt = x[3];
  ParseBoolValue(x[4], &ntf->rd);
  ntf->crtTs = ToTimestamp(x[5]);
}

std::string RowOf(const domain::AuditLog& lg) {
  return MakeRow({Text(lg.id), Text(lg.uid), lg.actn, TextEnum(lg.res), lg.det,
                  TextTime(lg.crtTs)});
}

void FillAuditLog(const Row& x, domain::AuditLog* lg) {
  if (x.size() < 6) {
    return;
  }
  ParseId(x[0], &lg->id);
  ParseId(x[1], &lg->uid);
  lg->actn = x[2];
  int res = 0;
  if (ParseIntValue(x[3], &res)) {
    lg->res = static_cast<domain::AuditResult>(res);
  }
  lg->det = x[4];
  lg->crtTs = ToTimestamp(x[5]);
}

std::string RowOf(const domain::IdempotencyRecord& rec) {
  return MakeRow({rec.ky, rec.op, rec.reqH, rec.res, TextTime(rec.crtTs)});
}

void FillIdempotencyRecord(const Row& x, domain::IdempotencyRecord* rec) {
  if (x.size() < 5) {
    return;
  }
  rec->ky = x[0];
  rec->op = x[1];
  rec->reqH = x[2];
  rec->res = x[3];
  rec->crtTs = ToTimestamp(x[4]);
}

std::string RowOf(const domain::BackgroundTask& tsk) {
  return MakeRow(
      {Text(tsk.id), tsk.tskTyp, tsk.pl, TextEnum(tsk.st), Text(tsk.retryN)});
}

void FillBackgroundTask(const Row& x, domain::BackgroundTask* tsk) {
  if (x.size() < 5) {
    return;
  }
  ParseId(x[0], &tsk->id);
  tsk->tskTyp = x[1];
  tsk->pl = x[2];
  int st = 0;
  if (ParseIntValue(x[3], &st)) {
    tsk->st = static_cast<domain::TaskStatus>(st);
  }
  ParseIntValue(x[4], &tsk->retryN);
}

domain::Id MaxId(const std::vector<domain::User>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::Activity>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::ActivityVersion>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::LogicalSession>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::Session>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::LogicalCategory>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::CategoryVersion>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::Registration>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::Ticket>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::CheckinEvent>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::Notification>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::AuditLog>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

domain::Id MaxId(const std::vector<domain::BackgroundTask>& xs) {
  domain::Id res = 0;
  for (std::size_t i = 0; i < xs.size(); ++i) {
    if (xs[i].id > res) {
      res = xs[i].id;
    }
  }
  return res;
}

}

DataStore::DataStore() { Clear(); }

void DataStore::Clear() {
  cnts_.clear();
  usrs_.clear();
  usrRls_.clear();
  acts_.clear();
  actVers_.clear();
  lSsns_.clear();
  ssns_.clear();
  lCats_.clear();
  catVers_.clear();
  ssnGrs_.clear();
  regs_.clear();
  tkts_.clear();
  chkEvs_.clear();
  ntfs_.clear();
  audLogs_.clear();
  idemRecs_.clear();
  bgTsks_.clear();
}

domain::Id DataStore::NextId(const std::string& tbl) {
  domain::Id& cur = cnts_[tbl];
  ++cur;
  return cur;
}

bool DataStore::Load(const std::string& dir) {
  const std::string pre = dir + "/";
  ReadTable(pre + "users.csv", [this](const Row& x) {
    domain::User usr;
    FillUser(x, &usr);
    usrs_.push_back(usr);
  });
  ReadTable(pre + "user_roles.csv", [this](const Row& x) {
    domain::UserRole usrRl;
    FillUserRole(x, &usrRl);
    usrRls_.push_back(usrRl);
  });
  ReadTable(pre + "activities.csv", [this](const Row& x) {
    domain::Activity act;
    FillActivity(x, &act);
    acts_.push_back(act);
  });
  ReadTable(pre + "activity_versions.csv", [this](const Row& x) {
    domain::ActivityVersion ver;
    FillActivityVersion(x, &ver);
    actVers_.push_back(ver);
  });
  ReadTable(pre + "logical_sessions.csv", [this](const Row& x) {
    domain::LogicalSession ssn;
    FillLogicalSession(x, &ssn);
    lSsns_.push_back(ssn);
  });
  ReadTable(pre + "sessions.csv", [this](const Row& x) {
    domain::Session ssn;
    FillSession(x, &ssn);
    ssns_.push_back(ssn);
  });
  ReadTable(pre + "logical_categories.csv", [this](const Row& x) {
    domain::LogicalCategory cat;
    FillLogicalCategory(x, &cat);
    lCats_.push_back(cat);
  });
  ReadTable(pre + "category_versions.csv", [this](const Row& x) {
    domain::CategoryVersion cat;
    FillCategoryVersion(x, &cat);
    catVers_.push_back(cat);
  });
  ReadTable(pre + "session_grants.csv", [this](const Row& x) {
    domain::SessionGrant grt;
    FillSessionGrant(x, &grt);
    ssnGrs_.push_back(grt);
  });
  ReadTable(pre + "registrations.csv", [this](const Row& x) {
    domain::Registration reg;
    FillRegistration(x, &reg);
    regs_.push_back(reg);
  });
  ReadTable(pre + "tickets.csv", [this](const Row& x) {
    domain::Ticket tkt;
    FillTicket(x, &tkt);
    tkts_.push_back(tkt);
  });
  ReadTable(pre + "checkin_events.csv", [this](const Row& x) {
    domain::CheckinEvent ev;
    FillCheckinEvent(x, &ev);
    chkEvs_.push_back(ev);
  });
  ReadTable(pre + "notifications.csv", [this](const Row& x) {
    domain::Notification ntf;
    FillNotification(x, &ntf);
    ntfs_.push_back(ntf);
  });
  ReadTable(pre + "audit_logs.csv", [this](const Row& x) {
    domain::AuditLog lg;
    FillAuditLog(x, &lg);
    audLogs_.push_back(lg);
  });
  ReadTable(pre + "idempotency_records.csv", [this](const Row& x) {
    domain::IdempotencyRecord rec;
    FillIdempotencyRecord(x, &rec);
    idemRecs_.push_back(rec);
  });
  ReadTable(pre + "background_tasks.csv", [this](const Row& x) {
    domain::BackgroundTask tsk;
    FillBackgroundTask(x, &tsk);
    bgTsks_.push_back(tsk);
  });
  RefreshCounters();
  return true;
}

bool DataStore::Save(const std::string& dir) const {
  std::error_code err;
  std::filesystem::create_directories(dir, err);
  const std::string pre = dir + "/";
  WriteTable<domain::User>(pre + "users.csv", usrs_, RowOf);
  WriteTable<domain::UserRole>(pre + "user_roles.csv", usrRls_, RowOf);
  WriteTable<domain::Activity>(pre + "activities.csv", acts_, RowOf);
  WriteTable<domain::ActivityVersion>(pre + "activity_versions.csv", actVers_,
                                      RowOf);
  WriteTable<domain::LogicalSession>(pre + "logical_sessions.csv", lSsns_,
                                     RowOf);
  WriteTable<domain::Session>(pre + "sessions.csv", ssns_, RowOf);
  WriteTable<domain::LogicalCategory>(pre + "logical_categories.csv", lCats_,
                                      RowOf);
  WriteTable<domain::CategoryVersion>(pre + "category_versions.csv", catVers_,
                                      RowOf);
  WriteTable<domain::SessionGrant>(pre + "session_grants.csv", ssnGrs_, RowOf);
  WriteTable<domain::Registration>(pre + "registrations.csv", regs_, RowOf);
  WriteTable<domain::Ticket>(pre + "tickets.csv", tkts_, RowOf);
  WriteTable<domain::CheckinEvent>(pre + "checkin_events.csv", chkEvs_, RowOf);
  WriteTable<domain::Notification>(pre + "notifications.csv", ntfs_, RowOf);
  WriteTable<domain::AuditLog>(pre + "audit_logs.csv", audLogs_, RowOf);
  WriteTable<domain::IdempotencyRecord>(pre + "idempotency_records.csv",
                                        idemRecs_, RowOf);
  WriteTable<domain::BackgroundTask>(pre + "background_tasks.csv", bgTsks_,
                                     RowOf);
  return true;
}

void DataStore::RefreshCounters() {
  cnts_["user"] = MaxId(usrs_);
  cnts_["activity"] = MaxId(acts_);
  cnts_["activity_version"] = MaxId(actVers_);
  cnts_["logical_session"] = MaxId(lSsns_);
  cnts_["session"] = MaxId(ssns_);
  cnts_["logical_category"] = MaxId(lCats_);
  cnts_["category_version"] = MaxId(catVers_);
  cnts_["registration"] = MaxId(regs_);
  cnts_["ticket"] = MaxId(tkts_);
  cnts_["checkin"] = MaxId(chkEvs_);
  cnts_["notification"] = MaxId(ntfs_);
  cnts_["audit_log"] = MaxId(audLogs_);
  cnts_["background_task"] = MaxId(bgTsks_);
}

}
}
