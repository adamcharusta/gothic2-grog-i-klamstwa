#pragma once
// Glue between aicore (UTF-8, story ids) and the Gothic engine (cp1250 text, script instances, waypoints).
#include <grog/ai/commands.h>
#include <grog/ai/session.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace grog::ai {

// UTF-8 -> Windows-1250 (the Polish Gothic fonts). Typographic punctuation becomes ASCII,
// characters outside cp1250 and invalid bytes become '?'.
std::string toGameText(std::string_view utf8);
// Windows-1250 -> UTF-8.
std::string fromGameText(std::string_view cp1250);

std::optional<std::string> npcIdForInstance(const GameData& data, std::string_view instance);
std::string instanceForNpc(const GameData& data, std::string_view npcId);

struct EngineAction {
  enum Kind { QuestEntry, GiveItemToPlayer, TakeItemFromPlayer, StartFight, NpcGoto, NpcApproachPlayer, NpcFightNpc,
              SpawnMonster, AmbientLine, EndGame };
  Kind kind = QuestEntry;
  std::string npcInstance, npc2Instance, itemInstance, waypoint, monsterInstance;
  std::string text;      // cp1250 (EndGame: outcome id)
  std::string textUtf8;  // the same text before the cp1250 conversion (for speech synthesis)
};

// Accepted verdicts only; commands with no engine effect (attitude, npc_knowledge) and unmapped ids are skipped.
std::vector<EngineAction> toEngineActions(const std::vector<Verdict>& verdicts, std::string_view speaker,
                                          const GameData& data);

}  // namespace grog::ai
