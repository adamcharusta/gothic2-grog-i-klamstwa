#include <grog/ai/world.h>

#include <algorithm>

namespace grog::ai {
using nlohmann::json;

WorldState WorldState::fromScenario(const Scenario& s, const Cast& cast) {
  WorldState w;
  w.scenario = s;
  for(auto& m : cast.npcs)
    w.npcs[m.id] = NpcState{};
  w.itemHolder[kTankard] = s.tankardLocation;
  return w;
}

std::vector<Fact> WorldState::factsKnownBy(std::string_view npc) const {
  std::vector<Fact> out;
  for(auto& f : facts)
    if(f.knownBy.empty() || std::find(f.knownBy.begin(), f.knownBy.end(), npc) != f.knownBy.end())
      out.push_back(f);
  return out;
}

std::set<std::string> WorldState::burnedNpcs() const {
  std::set<std::string> out;
  for(auto& [id, st] : npcs)
    if(st.attitude <= -3)
      out.insert(id);
  return out;
}

json toJson(const WorldState& w) {
  json npcs = json::object();
  for(auto& [id, st] : w.npcs) {
    json mem = json::array();
    for(auto& m : st.memory)
      mem.push_back({{"role", m.role == ChatMessage::User ? "user" : "assistant"}, {"text", m.text}});
    npcs[id] = {{"attitude", st.attitude}, {"knows", st.knows}, {"memory", mem}, {"summary", st.summary},
                {"knocked_out", st.knockedOutByPlayer}};
  }
  json facts = json::array();
  for(auto& f : w.facts)
    facts.push_back({{"t", f.gameTimeS}, {"text", f.text}, {"known_by", f.knownBy}});
  json quests = json::array();
  for(auto& q : w.quests)
    quests.push_back({{"quest", q.quest}, {"text", q.text}, {"t", q.gameTimeS}});
  return {{"version", kWorldSaveVersion},
          {"scenario", toJson(w.scenario)},
          {"npcs", npcs},
          {"items", w.itemHolder},
          {"facts", facts},
          {"quests", quests},
          {"events_this_hour", w.eventsThisHour},
          {"active_monster", w.activeMonster}};
}

Result<WorldState> worldFromJson(const json& j) {
  if(j.value("version", 0) != kWorldSaveVersion)
    return Error{ErrorKind::Io, "save data version " + std::to_string(j.value("version", 0)) + " is not supported (expected " +
                                    std::to_string(kWorldSaveVersion) + ")"};
  WorldState w;
  try {
    auto sc = scenarioFromJson(j.at("scenario"));
    if(!sc.ok())
      return sc.error();
    w.scenario = sc.value();
    for(auto& [id, n] : j.at("npcs").items()) {
      NpcState st;
      st.attitude = n.at("attitude").get<int>();
      st.knows = n.at("knows").get<std::vector<std::string>>();
      for(auto& m : n.at("memory"))
        st.memory.push_back({m.at("role") == "user" ? ChatMessage::User : ChatMessage::Assistant,
                             m.at("text").get<std::string>()});
      st.summary = n.at("summary").get<std::string>();
      st.knockedOutByPlayer = n.at("knocked_out").get<bool>();
      w.npcs[id] = std::move(st);
    }
    w.itemHolder = j.at("items").get<std::map<std::string, std::string>>();
    for(auto& f : j.at("facts"))
      w.facts.push_back({f.at("t").get<int64_t>(), f.at("text").get<std::string>(),
                         f.at("known_by").get<std::vector<std::string>>()});
    for(auto& q : j.at("quests"))
      w.quests.push_back({q.at("quest").get<std::string>(), q.at("text").get<std::string>(), q.at("t").get<int64_t>()});
    w.eventsThisHour = j.at("events_this_hour").get<int>();
    w.activeMonster = j.at("active_monster").get<std::string>();
  } catch(const json::exception& e) {
    return Error{ErrorKind::Io, std::string("save data: ") + e.what()};
  }
  return w;
}

}  // namespace grog::ai
