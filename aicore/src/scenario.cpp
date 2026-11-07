#include <grog/ai/rng.h>
#include <grog/ai/scenario.h>

#include <algorithm>
#include <deque>

namespace grog::ai {
using nlohmann::json;

const char* scenarioTypeName(ScenarioType t) {
  switch(t) {
    case ScenarioType::LostAtDice: return "lost_at_dice";
    case ScenarioType::Lost: return "lost";
    case ScenarioType::Stolen: return "stolen";
    case ScenarioType::InHisHut: return "in_his_hut";
  }
  return "unknown";
}

const char* roleName(Role r) {
  switch(r) {
    case Role::Culprit: return "culprit";
    case Role::Witness: return "witness";
    case Role::Liar: return "liar";
    case Role::Gossip: return "gossip";
    case Role::Unaware: return "unaware";
  }
  return "unknown";
}

std::string Cast::displayName(const std::string& npcId) const {
  for(auto& m : npcs)
    if(m.id == npcId)
      return m.name;
  return npcId;
}
bool Cast::hasNpc(const std::string& npcId) const {
  return std::any_of(npcs.begin(), npcs.end(), [&](auto& m) { return m.id == npcId; });
}
std::string Cast::aiNameOf(const std::string& itemId) const {
  auto it = itemAiName.find(itemId);
  return it == itemAiName.end() ? itemId : it->second;
}
std::string Cast::itemIdForAiName(const std::string& aiName) const {
  for(auto& [id, name] : itemAiName)
    if(name == aiName)
      return id;
  return {};
}
bool Cast::hasPlace(const std::string& placeId) const {
  return std::find(places.begin(), places.end(), placeId) != places.end();
}

Result<Cast> loadCast(const json& cast, const json& places) {
  Cast c;
  if(cast.value("schema", 0) != 1 || places.value("schema", 0) != 1)
    return Error{ErrorKind::Io, "cast.json/places.json: unsupported schema"};
  c.questGiver = cast.value("quest_giver", "");
  for(auto& n : cast.value("npcs", json::array()))
    c.npcs.push_back({n.value("id", ""), n.value("name", ""), n.value("instance", "")});
  for(auto& m : cast.value("motives", json::array()))
    c.motives.push_back(m.get<std::string>());
  for(auto& p : places.value("places", json::array())) {
    std::string id = p.value("id", "");
    c.places.push_back(id);
    c.placeWaypoints[id] = p.value("waypoint", "");
    const json names = p.value("name", json::object());  // keep alive: items() must not iterate a temporary
    for(auto& [lang, name] : names.items())
      c.placeNames[id][lang] = name.get<std::string>();
  }
  if(c.questGiver.empty() || !c.hasNpc(c.questGiver))
    return Error{ErrorKind::Io, "cast.json: quest_giver must be one of the npcs"};
  if(c.npcs.size() < 8)
    return Error{ErrorKind::Io, "cast.json: at least 8 npcs are needed for two clue paths plus liars"};
  if(!c.hasPlace("greg_hut"))
    return Error{ErrorKind::Io, "places.json: place 'greg_hut' is required"};
  if(c.motives.empty())
    return Error{ErrorKind::Io, "cast.json: motives must not be empty"};
  return c;
}

namespace {

std::string fill(std::string tpl, const std::string& x, const std::string& y) {
  auto rep = [&](const std::string& key, const std::string& val) {
    for(size_t p; (p = tpl.find(key)) != std::string::npos;)
      tpl.replace(p, key.size(), val);
  };
  rep("{X}", "{" + x + "}");
  rep("{Y}", "{" + y + "}");
  return tpl;
}

const std::vector<std::string> kGregLeads = {
    "{greg} remembers that {Y} was hanging around his hut the evening the tankard vanished.",
    "{greg} recalls drinking with {Y} the night the tankard disappeared.",
};
const std::vector<std::string> kWitnessLeads = {
    "{X} saw {Y} near Greg's hut late that night, looking nervous.",
    "{X} heard {Y} talking about Greg's tankard when they thought nobody was listening.",
    "{X} owes {Y} money and knows {Y} was the last one at Greg's table that night.",
};
const std::string kDiceFinal = "{X} watched {greg}, very drunk, lose a game of dice to {Y}; the stake was his tankard.";
const std::string kStolenFinal = "{X} saw {Y} sneak out of Greg's hut with something hidden under the coat.";
const std::string kFoundFinal = "{X} saw {Y} pick up a tankard that looked exactly like Greg's and hide it.";
const std::string kPlaceFinal = "{X} noticed something shiny, like a tankard, lying near {Y}.";
const std::string kHutFinal = "{X} saw {greg}, dead drunk, carry his tankard back into his own hut and collapse.";
const std::string kLie = "{X} claims (falsely) that {Y} took Greg's tankard.";
const std::string kGossip = "{X} heard a rumour (untrue) that {Y} has been drinking from a fancy new tankard.";

}  // namespace

Scenario rollScenario(uint64_t seed, const Cast& cast) {
  Rng rng(seed);
  Scenario s;
  s.seed = seed;
  s.questGiver = cast.questGiver;
  const std::string& greg = cast.questGiver;

  std::vector<std::string> pool;
  for(auto& m : cast.npcs) {
    s.roles[m.id] = Role::Unaware;
    if(m.id != greg)
      pool.push_back(m.id);
  }
  rng.shuffle(pool);
  auto take = [&]() {
    std::string v = pool.back();
    pool.pop_back();
    return v;
  };

  s.type = static_cast<ScenarioType>(rng.uniform(4));
  std::string finalTpl;
  switch(s.type) {
    case ScenarioType::LostAtDice:
      s.culprit = take();
      s.motive = "bet";
      s.tankardLocation = s.culprit;
      finalTpl = kDiceFinal;
      break;
    case ScenarioType::Stolen:
      s.culprit = take();
      s.motive = rng.pick(cast.motives);
      s.tankardLocation = s.culprit;
      finalTpl = kStolenFinal;
      break;
    case ScenarioType::Lost:
      if(rng.uniform(2) == 0) {
        s.culprit = take();
        s.motive = "found";
        s.tankardLocation = s.culprit;
        finalTpl = kFoundFinal;
      } else {
        std::vector<std::string> outside;
        for(auto& p : cast.places)
          if(p != "greg_hut")
            outside.push_back(p);
        s.motive = "accident";
        s.tankardLocation = "place:" + rng.pick(outside);
        finalTpl = kPlaceFinal;
      }
      break;
    case ScenarioType::InHisHut:
      s.culprit = greg;
      s.motive = "forgot";
      s.tankardLocation = "place:greg_hut";
      finalTpl = kHutFinal;
      break;
  }
  if(!s.culprit.empty() && s.culprit != greg)
    s.roles[s.culprit] = Role::Culprit;

  // Two node-disjoint chains of witnesses from Greg to the truth.
  int clueNo = 0;
  auto addClue = [&](const std::string& holder, std::string fact, const std::string& target, bool truthful) {
    Clue c;
    c.id = (truthful ? "c" : "x") + std::to_string(++clueNo);
    c.holderNpc = holder;
    c.fact = std::move(fact);
    c.pointsTo = {target};
    c.truthful = truthful;
    s.clues.push_back(std::move(c));
  };
  for(int chain = 0; chain < 2; ++chain) {
    int len = 1 + static_cast<int>(rng.uniform(2));
    std::vector<std::string> links;
    for(int i = 0; i < len; ++i) {
      links.push_back(take());
      s.roles[links.back()] = Role::Witness;
    }
    addClue(greg, fill(rng.pick(kGregLeads), greg, links[0]), links[0], true);
    for(size_t i = 0; i + 1 < links.size(); ++i)
      addClue(links[i], fill(rng.pick(kWitnessLeads), links[i], links[i + 1]), links[i + 1], true);
    addClue(links.back(), fill(finalTpl, links.back(), s.tankardLocation), s.tankardLocation, true);
  }

  // Liars and a gossip among the remaining pirates; their stories point at innocents.
  int liars = 1 + static_cast<int>(rng.uniform(2));
  for(int i = 0; i < liars && pool.size() >= 2; ++i) {
    std::string liar = take();
    s.roles[liar] = Role::Liar;
    std::string victim = rng.pick(pool);  // one pick: argument evaluation order is unspecified
    addClue(liar, fill(kLie, liar, victim), victim, false);
  }
  if(pool.size() >= 2) {
    std::string gossip = take();
    s.roles[gossip] = Role::Gossip;
    std::string victim = rng.pick(pool);
    addClue(gossip, fill(kGossip, gossip, victim), victim, false);
  }
  return s;
}

bool isSolvable(const Scenario& s, const std::set<std::string>& burned) {
  // Quest giver is the root (never burned).
  const std::string& root = s.questGiver;
  std::set<std::string> reached{root};
  std::deque<std::string> todo{root};
  while(!todo.empty()) {
    std::string node = todo.front();
    todo.pop_front();
    if(node == s.tankardLocation)
      return true;
    if(node != root && burned.count(node))
      continue;
    for(auto& c : s.clues) {
      if(!c.truthful || c.holderNpc != node)
        continue;
      for(auto& t : c.pointsTo)
        if(reached.insert(t).second)
          todo.push_back(t);
    }
  }
  return false;
}

json toJson(const Scenario& s) {
  json roles = json::object();
  for(auto& [k, v] : s.roles)
    roles[k] = roleName(v);
  json clues = json::array();
  for(auto& c : s.clues)
    clues.push_back({{"id", c.id}, {"holder", c.holderNpc}, {"fact", c.fact}, {"points_to", c.pointsTo},
                     {"truthful", c.truthful}});
  return {{"seed", s.seed},       {"quest_giver", s.questGiver}, {"type", scenarioTypeName(s.type)}, {"culprit", s.culprit},
          {"motive", s.motive},   {"tankard_location", s.tankardLocation}, {"roles", roles},
          {"clues", clues}};
}

Result<Scenario> scenarioFromJson(const json& j) {
  Scenario s;
  try {
    s.seed = j.at("seed").get<uint64_t>();
    s.questGiver = j.at("quest_giver").get<std::string>();
    std::string t = j.at("type").get<std::string>();
    bool found = false;
    for(auto st : {ScenarioType::LostAtDice, ScenarioType::Lost, ScenarioType::Stolen, ScenarioType::InHisHut})
      if(t == scenarioTypeName(st)) {
        s.type = st;
        found = true;
      }
    if(!found)
      return Error{ErrorKind::Io, "scenario: unknown type " + t};
    s.culprit = j.at("culprit").get<std::string>();
    s.motive = j.at("motive").get<std::string>();
    s.tankardLocation = j.at("tankard_location").get<std::string>();
    for(auto& [k, v] : j.at("roles").items()) {
      std::string r = v.get<std::string>();
      Role role = Role::Unaware;
      for(auto rr : {Role::Culprit, Role::Witness, Role::Liar, Role::Gossip, Role::Unaware})
        if(r == roleName(rr))
          role = rr;
      s.roles[k] = role;
    }
    for(auto& c : j.at("clues")) {
      Clue cl;
      cl.id = c.at("id").get<std::string>();
      cl.holderNpc = c.at("holder").get<std::string>();
      cl.fact = c.at("fact").get<std::string>();
      cl.pointsTo = c.at("points_to").get<std::vector<std::string>>();
      cl.truthful = c.at("truthful").get<bool>();
      s.clues.push_back(std::move(cl));
    }
  } catch(const json::exception& e) {
    return Error{ErrorKind::Io, std::string("scenario: ") + e.what()};
  }
  return s;
}

}  // namespace grog::ai
