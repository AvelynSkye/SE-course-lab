

#include "domain/datetime.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace campus {
namespace domain {

namespace {

std::tm ToLocalTime(Timestamp val) {
  std::tm tm = {};
#ifdef _WIN32
  localtime_s(&tm, &val);
#else
  localtime_r(&val, &tm);
#endif
  return tm;
}

}

Timestamp Now() { return std::time(nullptr); }

Timestamp AddMinutes(Timestamp base, std::int64_t minutes) {
  const std::int64_t seconds = minutes * static_cast<std::int64_t>(60);
  return static_cast<Timestamp>(static_cast<std::int64_t>(base) + seconds);
}

std::string FormatTime(Timestamp val) {
  if (val <= 0) {
    return "-";
  }
  std::tm tm = ToLocalTime(val);
  char buf[32] = {0};
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
  return std::string(buf);
}

bool ParseTime(const std::string& txt, Timestamp* val) {
  std::tm tm = {};
  std::istringstream ss(txt);
  ss >> std::get_time(&tm, "%Y-%m-%d %H:%M");
  if (ss.fail()) {
    return false;
  }
  tm.tm_isdst = -1;
  const Timestamp prs = std::mktime(&tm);
  if (prs == static_cast<Timestamp>(-1)) {
    return false;
  }
  *val = prs;
  return true;
}

}
}
