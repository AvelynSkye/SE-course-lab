

#include "service/service_result.h"

#include <string>

namespace campus {
namespace service {

ServiceResult ServiceResult::Ok(const std::string& msg) {
  ServiceResult res;
  res.ok = true;
  res.msg = msg;
  return res;
}

ServiceResult ServiceResult::Fail(const std::string& msg) {
  ServiceResult res;
  res.ok = false;
  res.msg = msg;
  return res;
}

}
}
