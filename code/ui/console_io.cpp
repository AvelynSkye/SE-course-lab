

#include "ui/console_io.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "domain/datetime.h"

namespace campus {
namespace ui {
namespace io {

std::string ReadLine(const std::string& pmt) {
  std::cout << pmt;
  std::string ln;
  if (!std::getline(std::cin, ln)) {
    std::cout << "\n输入结束，程序退出。" << std::endl;
    std::exit(0);
  }
  return ln;
}

domain::Id ReadId(const std::string& pmt) {
  while (true) {
    const std::string txt = ReadLine(pmt);
    std::istringstream ss(txt);
    domain::Id val = 0;
    ss >> val;
    if (!ss.fail()) {
      return val;
    }
    PrintFail("请输入合法的数字 ID");
  }
}

int ReadInt(const std::string& pmt) {
  while (true) {
    const std::string txt = ReadLine(pmt);
    std::istringstream ss(txt);
    int val = 0;
    ss >> val;
    if (!ss.fail()) {
      return val;
    }
    PrintFail("请输入合法整数");
  }
}

domain::Timestamp ReadTime(const std::string& pmt) {
  while (true) {
    const std::string txt = ReadLine(pmt);
    domain::Timestamp val = 0;
    if (domain::ParseTime(txt, &val)) {
      return val;
    }
    PrintFail("时间格式应为 YYYY-MM-DD HH:MM");
  }
}

bool Confirm(const std::string& pmt) {
  const std::string txt = ReadLine(pmt + "（1 确认 / 0 取消）: ");
  return txt == "1";
}

void PrintOk(const std::string& txt) {
  std::cout << "[成功] " << txt << std::endl;
}

void PrintFail(const std::string& txt) {
  std::cout << "[失败] " << txt << std::endl;
}

void PrintInfo(const std::string& txt) { std::cout << txt << std::endl; }

}
}
}
