

#ifndef SERVICE_STATS_SERVICE_H_
#define SERVICE_STATS_SERVICE_H_

#include <string>
#include <vector>

#include "domain/types.h"
#include "service/service_result.h"

namespace campus {
namespace infra {
class DataStore;
}

namespace service {

class RoleService;

struct StatResult {
  domain::Id aid = 0;
  std::string ttl;
  int regN = 0;
  int chkN = 0;
  int wlN = 0;
  int cncl = 0;
  double attRt = 0.0;

  std::string ToString() const;
};

class StatsService {
 public:
  StatsService(infra::DataStore* db, RoleService* rls);

  std::vector<StatResult> QueryStats(domain::Id uid, domain::Id aid);
  ServiceResult ExportRoster(domain::Id uid, domain::Id aid,
                             const std::string& pth, std::string* sum);

 private:
  infra::DataStore* db_;
  RoleService* rls_;
};

}
}

#endif
