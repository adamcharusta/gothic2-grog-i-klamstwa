#pragma once
#include <grog/ai/cards.h>
#include <grog/ai/llm.h>
#include <grog/ai/scenario.h>
#include <grog/ai/world.h>

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>

namespace grog::ai {

inline constexpr size_t kMemoryWindow = 12;

// "{skip} saw {greg} near {place:campfire}" -> "Skip saw Greg near ognisko w obozie".
std::string renderFact(std::string_view fact, const Cast& cast, Language lang);

// Actor = one NPC. Contains only what this NPC knows (never the full truth).
LlmRequest buildActorRequest(const NpcCard& card, const WorldState& world, const Cast& cast, const Lore& lore,
                             Language lang, std::string_view playerLine, std::string_view rulesText);

// Director = Game Master. Contains the full truth, clue graph and world summary.
LlmRequest buildDirectorRequest(const WorldState& world, const Cast& cast, const Lore& lore, std::string_view trigger,
                                std::string_view rulesText);

nlohmann::json actorReplySchema();
nlohmann::json directorReplySchema();

}  // namespace grog::ai
