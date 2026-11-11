#pragma once
#include <grog/ai/config.h>
#include <grog/ai/result.h>

#include <string>

namespace grog::ai {

// Short, player-facing explanation of a provider error (UTF-8), or "" when the player can't do anything about it.
// Never contains configuration values.
std::string playerHint(ErrorKind kind, Language lang);

}  // namespace grog::ai
