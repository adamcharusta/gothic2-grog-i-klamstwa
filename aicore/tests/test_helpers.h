#pragma once
#include <grog/ai/io.h>

#include <string>

namespace testutil {

// Repo path is compiled in as UTF-8; the project path contains "ł" and "–".
inline std::string repoFile(const std::string& rel) {
  auto r = grog::ai::readTextFile(grog::ai::pathFromUtf8(std::string(GROG_REPO_ROOT) + "/" + rel));
  return r.ok() ? r.value() : std::string();
}

inline std::string testData(const std::string& rel) {
  auto r = grog::ai::readTextFile(grog::ai::pathFromUtf8(std::string(GROG_TEST_DATA) + "/" + rel));
  return r.ok() ? r.value() : std::string();
}

}  // namespace testutil
