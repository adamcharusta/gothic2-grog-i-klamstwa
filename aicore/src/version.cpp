#include <grog/ai/version.h>

#ifndef GROG_AICORE_VERSION
#define GROG_AICORE_VERSION "0.0.0"
#endif

namespace grog::ai {

std::string_view version() { return GROG_AICORE_VERSION; }

}  // namespace grog::ai
