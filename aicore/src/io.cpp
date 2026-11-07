#include <grog/ai/io.h>

#include <fstream>
#include <sstream>

namespace grog::ai {

std::filesystem::path pathFromUtf8(std::string_view utf8) {
  std::u8string s(utf8.begin(), utf8.end());
  return std::filesystem::path(s);
}

std::string pathToUtf8(const std::filesystem::path& p) {
  auto s = p.u8string();
  return std::string(s.begin(), s.end());
}

Result<std::string> readTextFile(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary);
  if(!f)
    return Error{ErrorKind::Io, "cannot open " + pathToUtf8(p)};
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

Result<bool> writeTextFile(const std::filesystem::path& p, std::string_view content) {
  std::error_code ec;
  if(p.has_parent_path())
    std::filesystem::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if(!f)
    return Error{ErrorKind::Io, "cannot write " + pathToUtf8(p)};
  f.write(content.data(), static_cast<std::streamsize>(content.size()));
  return true;
}

}  // namespace grog::ai
