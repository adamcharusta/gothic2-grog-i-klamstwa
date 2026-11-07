#pragma once
#include <grog/ai/result.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace grog::ai {

// UTF-8 string -> filesystem path. Narrow paths on MSVC are ANSI, which breaks on "ł", "–" etc.
std::filesystem::path pathFromUtf8(std::string_view utf8);
std::string pathToUtf8(const std::filesystem::path& p);

Result<std::string> readTextFile(const std::filesystem::path& p);
Result<bool> writeTextFile(const std::filesystem::path& p, std::string_view content);

}  // namespace grog::ai
