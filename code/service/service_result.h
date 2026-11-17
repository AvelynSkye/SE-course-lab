

#ifndef SERVICE_SERVICE_RESULT_H_
#define SERVICE_SERVICE_RESULT_H_

#include <string>

namespace campus {
namespace service {

struct ServiceResult {
  bool ok = false;
  std::string msg;

  static ServiceResult Ok(const std::string& msg);
  static ServiceResult Fail(const std::string& msg);
};

}
}

#endif
