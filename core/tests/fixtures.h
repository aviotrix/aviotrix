#pragma once
#include <string>

namespace test {
inline std::string fixturePath(const char* name) {
  return std::string(AVIOTRIX_FIXTURES_DIR) + "/" + name;
}
}  // namespace test
