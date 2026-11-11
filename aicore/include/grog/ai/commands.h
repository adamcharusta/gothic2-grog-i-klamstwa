#pragma once
#include <grog/ai/result.h>
#include <grog/ai/scenario.h>
#include <grog/ai/world.h>

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace grog::ai {

inline constexpr const char* kDirector = "director";  // speaker id for Game Master commands
inline constexpr size_t kMaxSpeechChars = 400;
inline constexpr size_t kMaxQuestChars = 200;
inline constexpr int kMaxEventsPerHour = 8;
inline constexpr int64_t kApproachCooldownS = 600;   // the same NPC approaches the player at most every 10 game minutes

struct Command {
  std::string type;
  nlohmann::json args;  // the command object without "type"
};

struct ActorReply {
  std::string speech;
  std::string gesture = "neutral";
  std::vector<Command> actions;
  bool endDialog = false;
};

struct DirectorReply {
  std::string thought;
  std::vector<Command> actions;
};

// Tolerant: strips code fences and prose, takes the first balanced {...}. Strict JSON inside.
Result<ActorReply> parseActorReply(std::string_view modelText);
Result<DirectorReply> parseDirectorReply(std::string_view modelText);

struct Verdict {
  Command command;
  bool accepted = false;
  std::string reason;  // why rejected (for logs/debug)
};

// "AI proposes, the game decides." speaker = npc id or kDirector.
// gameTimeS is used for time-based limits (approach cooldown).
std::vector<Verdict> validate(const std::vector<Command>& commands, std::string_view speaker, const WorldState& world,
                              const Cast& cast, int64_t gameTimeS = 0);

// Applies accepted verdicts to the world. Engine-side effects (fights, movement, spawns) are
// executed by the caller from the same verdicts.
void apply(const std::vector<Verdict>& verdicts, std::string_view speaker, WorldState& world, int64_t gameTimeS);

bool breaksCharacter(std::string_view speech);

size_t utf8Length(std::string_view s);
std::string utf8Truncate(std::string_view s, size_t maxChars);

}  // namespace grog::ai
