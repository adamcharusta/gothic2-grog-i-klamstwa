#include <grog/ai/commands.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>

namespace grog::ai {
using nlohmann::json;

size_t utf8Length(std::string_view s) {
  size_t n = 0;
  for(unsigned char c : s)
    if((c & 0xC0) != 0x80)
      ++n;
  return n;
}

std::string utf8Truncate(std::string_view s, size_t maxChars) {
  size_t chars = 0;
  for(size_t i = 0; i < s.size(); ++i) {
    if((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
      if(chars == maxChars)
        return std::string(s.substr(0, i));
      ++chars;
    }
  }
  return std::string(s);
}

namespace {

// First balanced {...} outside of strings, or empty.
std::string_view firstObject(std::string_view t) {
  size_t start = t.find('{');
  if(start == std::string_view::npos)
    return {};
  int depth = 0;
  bool inStr = false, esc = false;
  for(size_t i = start; i < t.size(); ++i) {
    char c = t[i];
    if(inStr) {
      if(esc)
        esc = false;
      else if(c == '\\')
        esc = true;
      else if(c == '"')
        inStr = false;
      continue;
    }
    if(c == '"')
      inStr = true;
    else if(c == '{')
      ++depth;
    else if(c == '}' && --depth == 0)
      return t.substr(start, i - start + 1);
  }
  return {};
}

Result<json> extractJson(std::string_view text) {
  std::string_view obj = firstObject(text);
  if(obj.empty())
    return Error{ErrorKind::BadResponse, "reply contains no JSON object"};
  json j = json::parse(obj, nullptr, false);
  if(j.is_discarded() || !j.is_object())
    return Error{ErrorKind::BadResponse, "reply JSON is malformed"};
  return j;
}

// Models often get the argument shape slightly wrong (seen in playtests): fix the harmless variants
// here so the validator judges the intent, not the typo. Anything else is left for the validator to reject.
void normalizeArgs(const std::string& type, json& a) {
  // {"type":"attitude","items":{"delta":1}} -> fields lifted to the top level (never overwriting).
  for(const char* wrap : {"items", "args", "params"}) {
    if(!a.contains(wrap) || !a[wrap].is_object() || type == "give_item" || type == "take_item")
      continue;
    json inner = a[wrap];
    a.erase(wrap);
    for(auto& [k, v] : inner.items())
      if(!a.contains(k))
        a[k] = v;
  }
  if(a.contains("delta")) {
    json& d = a["delta"];
    if(d.is_string()) {
      const std::string s = d.get<std::string>();
      char* end = nullptr;
      long v = std::strtol(s.c_str(), &end, 10);
      if(!s.empty() && end && *end == 0)
        d = v;
    } else if(d.is_number_float()) {
      double v = d.get<double>();
      if(v == static_cast<double>(static_cast<int64_t>(v)))
        d = static_cast<int64_t>(v);
    }
  }
  const bool wantsText = type == "quest_entry" || type == "quest_new" || type == "ambient_line" ||
                         type == "npc_approach_player";
  if(wantsText && !(a.contains("text") && a["text"].is_string()))
    for(const char* alias : {"fact", "entry", "line", "speech"})
      if(a.contains(alias) && a[alias].is_string()) {
        a["text"] = a[alias];
        break;
      }
}

Result<std::vector<Command>> readActions(const json& j, bool required) {
  std::vector<Command> out;
  if(!j.contains("actions")) {
    if(required)
      return Error{ErrorKind::BadResponse, "reply has no 'actions'"};
    return out;
  }
  if(!j["actions"].is_array())
    return Error{ErrorKind::BadResponse, "'actions' is not an array"};
  for(auto& a : j["actions"]) {
    if(!a.is_object() || !a.contains("type") || !a["type"].is_string())
      continue;  // skip junk entries, keep the rest
    Command c{a["type"].get<std::string>(), a};
    c.args.erase("type");
    normalizeArgs(c.type, c.args);
    out.push_back(std::move(c));
  }
  return out;
}

const std::set<std::string> kMonsters = {"swampshark", "bloodfly", "lurker", "waran"};
const std::set<std::string> kSpawnPlaces = {"beach", "palisade_gate", "lookout"};
const std::set<std::string> kEventTypes = {"npc_goto", "npc_approach_player", "npc_fight_npc", "gather",
                                           "spawn_monster", "ambient_line"};

std::string str(const json& a, const char* k) {
  return a.contains(k) && a[k].is_string() ? a[k].get<std::string>() : std::string();
}

}  // namespace

Result<ActorReply> parseActorReply(std::string_view modelText) {
  auto j = extractJson(modelText);
  if(!j.ok())
    return j.error();
  const json& o = j.value();
  if(!o.contains("speech") || !o["speech"].is_string() || o["speech"].get<std::string>().empty())
    return Error{ErrorKind::BadResponse, "reply has no 'speech'"};
  ActorReply r;
  r.speech = utf8Truncate(o["speech"].get<std::string>(), kMaxSpeechChars);
  static const std::set<std::string> kGestures = {"neutral", "angry", "laugh", "think", "shrug", "threaten", "drink"};
  if(o.contains("gesture") && o["gesture"].is_string() && kGestures.count(o["gesture"].get<std::string>()))
    r.gesture = o["gesture"].get<std::string>();
  auto acts = readActions(o, false);
  if(!acts.ok())
    return acts.error();
  r.actions = std::move(acts.value());
  r.endDialog = o.contains("end_dialog") && o["end_dialog"].is_boolean() && o["end_dialog"].get<bool>();
  return r;
}

Result<DirectorReply> parseDirectorReply(std::string_view modelText) {
  auto j = extractJson(modelText);
  if(!j.ok())
    return j.error();
  DirectorReply r;
  r.thought = str(j.value(), "thought");
  auto acts = readActions(j.value(), true);
  if(!acts.ok())
    return acts.error();
  r.actions = std::move(acts.value());
  return r;
}

std::vector<Verdict> validate(const std::vector<Command>& commands, std::string_view speaker, const WorldState& w,
                              const Cast& cast, int64_t gameTimeS) {
  std::vector<Verdict> out;
  const bool isDirector = speaker == kDirector;
  int events = w.eventsThisHour;
  bool monsterActive = !w.activeMonster.empty();
  std::map<std::string, std::string> holders = w.itemHolder;  // simulate gives within the batch
  std::map<std::string, int64_t> approached = w.lastApproachS;  // simulate approaches within the batch

  for(auto& c : commands) {
    Verdict v{c, false, ""};
    const json& a = c.args;
    auto knownNpc = [&](const std::string& id) { return cast.hasNpc(id); };
    auto reject = [&](std::string why) { v.reason = std::move(why); };

    if(kEventTypes.count(c.type) && events >= kMaxEventsPerHour) {
      reject("event limit per hour reached");
    } else if(c.type == "attitude") {
      if(isDirector)
        reject("only an NPC can change its own attitude");
      else if(!a.contains("delta") || !a["delta"].is_number_integer())
        reject("delta must be an integer");
      else if(a["delta"].is_number_unsigned() ? a["delta"].get<uint64_t>() > 2
                                               : (a["delta"].get<int64_t>() < -2 || a["delta"].get<int64_t>() > 2))
        reject("delta out of range -2..2");
      else
        v.accepted = true;
    } else if(c.type == "quest_entry" || c.type == "quest_new") {
      std::string t = str(a, "text");
      if(a.contains("quest") && !a["quest"].is_string())
        reject("quest must be a string");
      else if(t.empty())
        reject("empty text");
      else if(utf8Length(t) > kMaxQuestChars)
        reject("text too long");
      else
        v.accepted = true;
    } else if(c.type == "give_item") {
      std::string item = str(a, "item");
      auto it = holders.find(item);
      if(isDirector)
        reject("the director cannot hand over items");
      else if(item.empty() || it == holders.end() || it->second != speaker)
        reject("speaker does not have item '" + item + "'");
      else {
        v.accepted = true;
        it->second = "player";
      }
    } else if(c.type == "take_item") {
      std::string item = str(a, "item");
      auto it = holders.find(item);
      if(isDirector)
        reject("the director cannot take items");
      else if(item.empty() || it == holders.end() || it->second != "player")
        reject("the player does not carry '" + item + "'");
      else {
        v.accepted = true;
        it->second = std::string(speaker);
      }
    } else if(c.type == "end_game") {
      std::string o = str(a, "outcome");
      auto held = [&](const char* item) { auto h = holders.find(item); return h != holders.end() && h->second == speaker; };
      if(speaker != cast.questGiver)
        reject("only the quest giver ends the game");
      else if(!w.ending.empty())
        reject("the game has already ended");
      else if(o == "returned" ? !held(kTankard)
              : (o == "fooled" || o == "caught") ? !held("tankard_lookalike")
              : o != "gave_up")
        reject("outcome '" + o + "' does not match what the quest giver holds");
      else
        v.accepted = true;
    } else if(c.type == "start_fight") {
      if(isDirector)
        reject("the director cannot start a fight with the player");
      else
        v.accepted = true;
    } else if(c.type == "npc_knowledge") {
      std::string fact = str(a, "fact");
      if(!isDirector)
        reject("only the director changes what other NPCs know");
      else if(!knownNpc(str(a, "npc")))
        reject("unknown npc");
      else if(fact.empty() || utf8Length(fact) > 300)
        reject("fact must be 1-300 characters");
      else
        v.accepted = true;
    } else if(c.type == "npc_goto") {
      if(!isDirector)
        reject("only the director moves NPCs");
      else if(!knownNpc(str(a, "npc")) || !cast.hasPlace(str(a, "place")))
        reject("unknown npc or place");
      else
        v.accepted = true;
    } else if(c.type == "npc_approach_player" || c.type == "ambient_line") {
      const std::string npc = str(a, "npc"), text = str(a, "text");
      auto last = approached.find(npc);
      if(!isDirector)
        reject("only the director triggers events");
      else if(npc.empty())
        reject(c.type + " needs the npc who speaks");
      else if(!knownNpc(npc))
        reject("unknown npc");
      else if(text.empty() || utf8Length(text) > 300)
        reject(c.type + " needs a text of 1-300 characters (what the npc says)");
      else if(c.type == "npc_approach_player" && last != approached.end() && gameTimeS - last->second < kApproachCooldownS)
        reject("this npc approached the player too recently");
      else {
        v.accepted = true;
        if(c.type == "npc_approach_player")
          approached[npc] = gameTimeS;
      }
    } else if(c.type == "npc_fight_npc") {
      std::string n1 = str(a, "npc"), n2 = str(a, "npc2");
      if(!isDirector)
        reject("only the director triggers events");
      else if(!knownNpc(n1) || !knownNpc(n2) || n1 == n2)
        reject("need two different known npcs");
      else
        v.accepted = true;
    } else if(c.type == "gather") {
      bool ok = isDirector && cast.hasPlace(str(a, "place")) && a.contains("npcs") && a["npcs"].is_array() &&
                !a["npcs"].empty();
      if(ok)
        for(auto& n : a["npcs"])
          ok = ok && n.is_string() && knownNpc(n.get<std::string>());
      if(ok)
        v.accepted = true;
      else
        reject("gather needs the director, a known place and known npcs");
    } else if(c.type == "spawn_monster") {
      if(!isDirector)
        reject("only the director spawns monsters");
      else if(!kMonsters.count(str(a, "monster")))
        reject("monster not allowed");
      else if(!kSpawnPlaces.count(str(a, "place")))
        reject("monsters may only appear outside the palisade");
      else if(monsterActive)
        reject("a monster is already active");
      else {
        v.accepted = true;
        monsterActive = true;
      }
    } else {
      reject("unknown command type");
    }

    if(v.accepted && kEventTypes.count(c.type))
      ++events;
    out.push_back(std::move(v));
  }
  return out;
}

void apply(const std::vector<Verdict>& verdicts, std::string_view speaker, WorldState& w, int64_t gameTimeS) {
  std::string who(speaker);
  for(auto& v : verdicts) {
    if(!v.accepted)
      continue;
    const json& a = v.command.args;
    const std::string& t = v.command.type;
    if(t == "attitude") {
      int& att = w.npcs[who].attitude;
      att = static_cast<int>(std::clamp<int64_t>(att + a["delta"].get<int64_t>(), -5, 5));
    } else if(t == "quest_entry" || t == "quest_new") {
      std::string q = str(a, "quest");
      w.quests.push_back({q.empty() ? std::string("tankard") : q, str(a, "text"), gameTimeS});
    } else if(t == "give_item") {
      w.itemHolder[str(a, "item")] = "player";
    } else if(t == "take_item") {
      w.itemHolder[str(a, "item")] = who;
    } else if(t == "end_game") {
      w.ending = str(a, "outcome");
      if((w.ending == "returned" || w.ending == "fooled") && w.itemHolder.count(kRewardKey) &&
         w.itemHolder[kRewardKey] == who)
        w.itemHolder[kRewardKey] = "player";
    } else if(t == "npc_knowledge") {
      w.npcs[str(a, "npc")].knows.push_back(str(a, "fact"));
    } else if(t == "spawn_monster") {
      w.activeMonster = str(a, "monster");
    } else if(t == "npc_approach_player") {
      // The NPC opens the conversation with this line: it is its own words from now on.
      const std::string npc = str(a, "npc");
      w.lastApproachS[npc] = gameTimeS;
      // Stored as a user/assistant pair so chat histories keep alternating roles (some providers require it).
      auto& mem = w.npcs[npc].memory;
      mem.push_back({ChatMessage::User, "(You walk up to the player and speak first.)"});
      mem.push_back({ChatMessage::Assistant, str(a, "text")});
    }
    if(kEventTypes.count(t))
      ++w.eventsThisHour;
  }
}

bool breaksCharacter(std::string_view speech) {
  std::string low(speech);
  std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  for(const char* p : {"as an ai", "language model", "jako ai", "model językowy", "modelem językowym", "chatgpt",
                       "claude", "openai", "anthropic", "i'm an ai", "jestem ai", "sztuczna inteligencja"})
    if(low.find(p) != std::string::npos)
      return true;
  return false;
}

}  // namespace grog::ai
