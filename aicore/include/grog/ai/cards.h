#pragma once
#include <grog/ai/config.h>
#include <grog/ai/result.h>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace grog::ai {

using LangText = std::map<Language, std::string>;
using LangLines = std::map<Language, std::vector<std::string>>;

// TTS voice: an OpenAI voice name and a delivery instruction per language ("pl"/"en").
// A card without a voice has an empty name (the session falls back to a default).
// `original` is the voice number of the NPC's actor in the original game (0 = unknown);
// cards sharing an actor share the OpenAI voice name.
struct NpcVoice {
  std::string name;
  int original = 0;
  std::map<std::string, std::string> style;
};

struct NpcCard {
  std::string id, name, campRole;
  LangText personality, speechStyle;
  LangLines sampleLines, farewells, fallbackLines;
  std::vector<std::string> knowsAbout;           // lore topics
  std::map<std::string, std::string> relations;  // npc id -> text
  std::string persuasion;
  std::vector<std::string> items;                // items the NPC starts with
  NpcVoice voice;
};

struct Lore {
  std::vector<std::pair<std::string, std::string>> entries;  // topic, text
};

// Rejects cards missing any pl/en personality, speech style, farewells or fallback lines.
Result<NpcCard> loadCard(const nlohmann::json& j);
Result<Lore> loadLore(const nlohmann::json& j);

}  // namespace grog::ai
