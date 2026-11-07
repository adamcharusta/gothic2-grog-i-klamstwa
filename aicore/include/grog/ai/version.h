#pragma once
#include <string_view>

namespace grog::ai {

// Mod version compiled into the library (from the repo VERSION file or GROG_AICORE_VERSION).
std::string_view version();

}  // namespace grog::ai
