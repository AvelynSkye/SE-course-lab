

#ifndef DOMAIN_DATETIME_H_
#define DOMAIN_DATETIME_H_

#include <string>

#include "domain/types.h"

namespace campus {
namespace domain {

Timestamp Now();
Timestamp AddMinutes(Timestamp base, std::int64_t minutes);
std::string FormatTime(Timestamp val);
bool ParseTime(const std::string& txt, Timestamp* val);

}
}

#endif
