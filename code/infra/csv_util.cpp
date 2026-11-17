

#include "infra/csv_util.h"

#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace campus {
namespace infra {

std::string Trim(const std::string& txt) {
  const std::string kWs = " \t\r\n";
  const std::size_t begin = txt.find_first_not_of(kWs);
  if (begin == std::string::npos) {
    return "";
  }
  const std::size_t end = txt.find_last_not_of(kWs);
  return txt.substr(begin, end - begin + 1);
}

std::string EscapeCsv(const std::string& fld) {
  const bool needQ = fld.find(',') != std::string::npos ||
                     fld.find('"') != std::string::npos ||
                     fld.find('\n') != std::string::npos;
  if (!needQ) {
    return fld;
  }
  std::string esc = "\"";
  for (std::size_t i = 0; i < fld.size(); ++i) {
    if (fld[i] == '"') {
      esc += "\"\"";
    } else {
      esc += fld[i];
    }
  }
  esc += "\"";
  return esc;
}

std::string JoinCsv(const std::vector<std::string>& flds) {
  std::string ln;
  for (std::size_t i = 0; i < flds.size(); ++i) {
    if (i > 0) {
      ln += ",";
    }
    ln += EscapeCsv(flds[i]);
  }
  return ln;
}

std::vector<std::string> SplitCsv(const std::string& ln) {
  std::vector<std::string> flds;
  std::string cur;
  bool qtd = false;
  for (std::size_t i = 0; i < ln.size(); ++i) {
    const char ch = ln[i];
    if (qtd) {
      if (ch == '"') {
        if (i + 1 < ln.size() && ln[i + 1] == '"') {
          cur += '"';
          ++i;
        } else {
          qtd = false;
        }
      } else {
        cur += ch;
      }
      continue;
    }
    if (ch == '"') {
      qtd = true;
    } else if (ch == ',') {
      flds.push_back(cur);
      cur.clear();
    } else {
      cur += ch;
    }
  }
  flds.push_back(cur);
  return flds;
}

bool ParseId(const std::string& txt, domain::Id* val) {
  if (txt.empty()) {
    return false;
  }
  std::istringstream ss(txt);
  domain::Id prs = 0;
  ss >> prs;
  if (ss.fail()) {
    return false;
  }
  *val = prs;
  return true;
}

bool ParseIntValue(const std::string& txt, int* val) {
  if (txt.empty()) {
    return false;
  }
  std::istringstream ss(txt);
  int prs = 0;
  ss >> prs;
  if (ss.fail()) {
    return false;
  }
  *val = prs;
  return true;
}

bool ParseBoolValue(const std::string& txt, bool* val) {
  if (txt == "1" || txt == "true") {
    *val = true;
    return true;
  }
  if (txt == "0" || txt == "false") {
    *val = false;
    return true;
  }
  return false;
}

}
}
