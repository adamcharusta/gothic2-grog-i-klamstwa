#pragma once
#include <grog/ai/config.h>
#include <grog/ai/result.h>

#include <string>

namespace grog::ai {

// Short, player-facing explanation of a provider error (UTF-8), or "" when the player can't do anything about it.
// Never contains configuration values.
std::string playerHint(ErrorKind kind, Language lang);

// Same for a voice (TTS) error: the text stays, only the audio is missing. "" when there is nothing to say.
std::string voiceHint(ErrorKind kind, Language lang);

}  // namespace grog::ai
