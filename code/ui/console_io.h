

#ifndef UI_CONSOLE_IO_H_
#define UI_CONSOLE_IO_H_

#include <string>

#include "domain/types.h"

namespace campus {
namespace ui {
namespace io {

std::string ReadLine(const std::string& pmt);
domain::Id ReadId(const std::string& pmt);
int ReadInt(const std::string& pmt);
domain::Timestamp ReadTime(const std::string& pmt);
bool Confirm(const std::string& pmt);

void PrintOk(const std::string& txt);
void PrintFail(const std::string& txt);
void PrintInfo(const std::string& txt);

}
}
}

#endif
