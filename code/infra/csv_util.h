

#ifndef INFRA_CSV_UTIL_H_
#define INFRA_CSV_UTIL_H_

#include <string>
#include <vector>

#include "domain/types.h"

namespace campus {
namespace infra {

std::string Trim(const std::string& txt);
std::vector<std::string> SplitCsv(const std::string& ln);
std::string JoinCsv(const std::vector<std::string>& flds);
std::string EscapeCsv(const std::string& fld);

bool ParseId(const std::string& txt, domain::Id* val);
bool ParseIntValue(const std::string& txt, int* val);
bool ParseBoolValue(const std::string& txt, bool* val);

}
}

#endif
