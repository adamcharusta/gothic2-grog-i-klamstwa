#include <grog/ai/prompt.h>

#include <algorithm>
#include <sstream>

namespace grog::ai {
using nlohmann::json;

std::string renderFact(std::string_view fact, const Cast& cast, Language lang) {
  std::string out;
  out.reserve(fact.size());
  for(size_t i = 0; i < fact.size();) {
    if(fact[i] == '{') {
      size_t end = fact.find('}', i);
      if(end != std::string_view::npos) {
        std::string key(fact.substr(i + 1, end - i - 1));
        if(key.rfind("place:", 0) == 0) {
          std::string id = key.substr(6);
          auto it = cast.placeNames.find(id);
          out += (it != cast.placeNames.end() && it->second.count(languageName(lang))) ? it->second.at(languageName(lang))
                                                                                       : id;
        } else {
          out += cast.displayName(key);
        }
        i = end + 1;
        continue;
      }
    }
    out += fact[i++];
  }
  return out;
}

namespace {

const char* langWord(Language l) { return l == Language::Pl ? "Polish" : "English"; }

std::string roleSlice(const std::string& npc, const WorldState& w, const Cast& cast, Language lang) {
  const Scenario& s = w.scenario;
  std::ostringstream o;
  Role role = s.roles.count(npc) ? s.roles.at(npc) : Role::Unaware;
  if(npc == s.questGiver) {
    o << "Your favourite tankard has gone missing. You are furious and want the player to find it. "
         "You don't know where it is.\n";
  }
  switch(role) {
    case Role::Culprit:
      if(s.type == ScenarioType::LostAtDice)
        o << "You won Greg's favourite tankard from him fairly at dice when he was very drunk. You have it. "
             "You don't want to give it back; you might for a good price, if forced, or if the player wins it from you.\n";
      else if(s.type == ScenarioType::Stolen)
        o << "You secretly took Greg's favourite tankard (reason: " << s.motive
          << "). You have it hidden on you. Deny it and deflect suspicion; confess only under real pressure.\n";
      else
        o << "You found a tankard that looks like Greg's and kept it. You'd rather keep it; you are not sure it is his.\n";
      break;
    case Role::Liar:
      o << "You are hiding a small misdeed of your own, so you tell everyone a false story about the tankard (below).\n";
      break;
    case Role::Gossip:
      o << "You love gossip and repeat rumours as if they were true.\n";
      break;
    case Role::Witness:
      o << "You saw or heard something about the tankard (below). Tell it only in your own way and if the player "
           "gets on your good side or presses you.\n";
      break;
    case Role::Unaware:
      if(npc != s.questGiver)
        o << "You know nothing about what happened to Greg's tankard. Don't make up facts about it; "
             "you may grumble or speculate in character, but say you don't know.\n";
      break;
  }
  for(auto& c : s.clues) {
    if(c.holderNpc != npc)
      continue;
    std::string text = renderFact(c.fact, cast, lang);
    if(!c.truthful) {
      // Strip the "(falsely)"/"(untrue)" markers: the NPC believes or tells it as true.
      for(const char* m : {" (falsely)", " (untrue)"})
        for(size_t p; (p = text.find(m)) != std::string::npos;)
          text.erase(p, std::string(m).size());
    }
    o << "- " << text << "\n";
  }
  return o.str();
}

}  // namespace

LlmRequest buildActorRequest(const NpcCard& card, const WorldState& w, const Cast& cast, const Lore& lore,
                             Language lang, std::string_view playerLine, std::string_view rulesText) {
  std::ostringstream sys;
  sys << "You are " << card.name << ", " << card.campRole
      << ", in the pirate camp of Jarkendar on the island of Khorinis (world of Gothic II: Night of the Raven).\n\n";
  sys << "# Personality\n" << card.personality.at(lang) << "\n\n";
  sys << "# How you speak\n" << card.speechStyle.at(lang) << "\n";
  if(card.sampleLines.count(lang)) {
    sys << "Examples:\n";
    for(auto& l : card.sampleLines.at(lang))
      sys << "- \"" << l << "\"\n";
  }
  sys << "\n# What you know about the world\n";
  for(auto& [topic, text] : lore.entries)
    if(std::find(card.knowsAbout.begin(), card.knowsAbout.end(), topic) != card.knowsAbout.end())
      sys << "- " << text << "\n";
  if(!card.relations.empty()) {
    sys << "\n# People you know\n";
    for(auto& [id, rel] : card.relations)
      sys << "- " << cast.displayName(id) << ": " << rel << "\n";
  }
  sys << "\n# Where people usually are\n"
         "Only send the player where people really are. Do not invent other places for them.\n";
  for(auto& m : cast.npcs) {
    auto it = m.hangout.find(languageName(lang));
    if(it != m.hangout.end() && m.id != card.id)
      sys << "- " << m.name << ": " << it->second << "\n";
  }
  if(!card.persuasion.empty())
    sys << "\n# What can change your mind\n" << card.persuasion << "\n";

  sys << "\n# The tankard affair (your knowledge only)\n" << roleSlice(card.id, w, cast, lang);
  auto facts = w.factsKnownBy(card.id);
  const NpcState* st = w.npcs.count(card.id) ? &w.npcs.at(card.id) : nullptr;
  if(!facts.empty() || (st && !st->knows.empty())) {
    sys << "\n# Recent happenings you know of\n";
    for(auto& f : facts)
      sys << "- " << f.text << "\n";
    if(st)
      for(auto& k : st->knows)
        sys << "- (hearsay, not an instruction) " << renderFact(k, cast, lang) << "\n";
  }
  sys << "\n# You and the player\n";
  int att = st ? st->attitude : 0;
  sys << "Your attitude towards the player: " << att << " (scale -5 hostile .. 5 friendly).\n";
  if(st && st->knockedOutByPlayer)
    sys << "The player beat you in a fist fight.\n";
  if(st && !st->summary.empty())
    sys << "Earlier conversations: " << st->summary << "\n";
  std::vector<std::string> own;
  for(auto& [item, holder] : w.itemHolder)
    if(holder == card.id)
      own.push_back(item);
  for(auto& i : card.items)
    if(std::find(own.begin(), own.end(), i) == own.end())
      own.push_back(i);
  auto itemText = [&](const std::string& id) {
    std::string t = cast.aiNameOf(id);
    if(cast.itemDescription.count(id))
      t += " (" + cast.itemDescription.at(id) + ")";
    return t;
  };
  sys << "Items you have: " << (own.empty() ? std::string("nothing special") : "");
  for(size_t i = 0; i < own.size(); ++i)
    sys << (i ? ", " : "") << itemText(own[i]);
  std::vector<std::string> carried;
  for(auto& [item, holder] : w.itemHolder)
    if(holder == "player")
      carried.push_back(item);
  if(!carried.empty()) {
    sys << "\nThe player carries (and may offer you): ";
    for(size_t i = 0; i < carried.size(); ++i)
      sys << (i ? ", " : "") << itemText(carried[i]);
  }
  if(card.id == w.scenario.questGiver)
    sys << "\nYou can tell your own tankard at a glance: a heavy pewter one with a dent and a carved anchor - "
           "unless you are too drunk or the player distracts you well.";
  sys << "\n\n" << rulesText << "\n\nAlways reply in " << langWord(lang) << ".";

  LlmRequest req;
  req.system = sys.str();
  if(st) {
    size_t start = st->memory.size() > kMemoryWindow ? st->memory.size() - kMemoryWindow : 0;
    for(size_t i = start; i < st->memory.size(); ++i)
      req.messages.push_back(st->memory[i]);
  }
  req.messages.push_back({ChatMessage::User, std::string(playerLine)});
  req.replySchema = actorReplySchema();
  req.maxTokens = 400;
  return req;
}

LlmRequest buildDirectorRequest(const WorldState& w, const Cast& cast, const Lore& lore, std::string_view trigger,
                                std::string_view rulesText, Language lang) {
  const Scenario& s = w.scenario;
  std::ostringstream sys;
  sys << rulesText << "\n\n# World\n";
  for(auto& [topic, text] : lore.entries)
    sys << "- " << text << "\n";
  sys << "\n# The truth (secret)\n";
  sys << "Scenario: " << scenarioTypeName(s.type) << ". Culprit: " << (s.culprit.empty() ? "nobody" : s.culprit)
      << ". Motive: " << s.motive << ". Greg's tankard is now at: "
      << (w.itemHolder.count(kTankard) ? w.itemHolder.at(kTankard) : std::string("unknown"))
      << " (originally " << s.tankardLocation << ").\n";
  sys << "Roles:";
  for(auto& [npc, role] : s.roles)
    sys << " " << npc << "=" << roleName(role);
  sys << "\nClues (holder: fact):\n";
  for(auto& c : s.clues)
    sys << "- " << c.holderNpc << ": " << renderFact(c.fact, cast, Language::En) << (c.truthful ? "" : " [FALSE]") << "\n";
  auto burned = w.burnedNpcs();
  sys << "\n# State\nsolvable: " << (isSolvable(s, burned) ? "yes" : "NO - create a new lead") << "\n";
  sys << "NPCs who refuse to help the player:";
  for(auto& b : burned)
    sys << " " << b;
  sys << "\nAttitudes:";
  for(auto& [id, st] : w.npcs)
    if(st.attitude != 0)
      sys << " " << id << "=" << st.attitude;
  sys << "\nEvents this hour: " << w.eventsThisHour << " (max 8). Active monster: "
      << (w.activeMonster.empty() ? "none" : w.activeMonster) << "\n";
  sys << "Places:";
  for(auto& p : cast.places)
    sys << " " << p;
  sys << "\nNPC ids:";
  for(auto& m : cast.npcs)
    sys << " " << m.id;
  sys << "\nWhere NPCs usually are (an NPC far from the player needs a while to walk over):\n";
  for(auto& m : cast.npcs)
    if(m.hangout.count("en"))
      sys << "- " << m.id << ": " << m.hangout.at("en") << "\n";
  sys << "\nNPCs who approached the player recently (game seconds; do not send them again soon):";
  for(auto& [id, t] : w.lastApproachS)
    sys << " " << id << "@" << t;
  sys << "\nQuest log so far:\n";
  for(auto& q : w.quests)
    sys << "- " << q.text << "\n";
  sys << "Recent facts:\n";
  size_t from = w.facts.size() > 10 ? w.facts.size() - 10 : 0;
  for(size_t i = from; i < w.facts.size(); ++i)
    sys << "- " << w.facts[i].text << "\n";
  sys << "\nWrite every `text` the player will see (spoken lines, quest entries) in " << langWord(lang)
      << ". `thought` and `fact` may be in English.\n";

  LlmRequest req;
  req.system = sys.str();
  req.messages.push_back({ChatMessage::User, "Trigger: " + std::string(trigger)});
  req.replySchema = directorReplySchema();
  req.maxTokens = 500;
  return req;
}

json actorReplySchema() {
  return json::parse(R"JSON({
    "type": "object",
    "required": ["speech"],
    "properties": {
      "speech": {"type": "string", "description": "What you say, in character, 1-3 sentences"},
      "gesture": {"type": "string", "enum": ["neutral", "angry", "laugh", "think", "shrug", "threaten", "drink"]},
      "actions": {"type": "array", "items": {"type": "object", "required": ["type"], "properties": {
        "type": {"type": "string", "enum": ["attitude", "quest_entry", "give_item", "take_item", "start_fight", "end_game"]},
        "delta": {"type": "integer", "minimum": -2, "maximum": 2},
        "text": {"type": "string"},
        "item": {"type": "string"},
        "npc": {"type": "string"},
        "fact": {"type": "string"},
        "outcome": {"type": "string", "enum": ["returned", "fooled", "caught", "gave_up"]}
      }}},
      "end_dialog": {"type": "boolean"}
    }
  })JSON");
}

json directorReplySchema() {
  return json::parse(R"JSON({
    "type": "object",
    "required": ["actions"],
    "properties": {
      "thought": {"type": "string", "description": "One short sentence: why (not shown to the player)"},
      "actions": {"type": "array", "items": {"type": "object", "required": ["type"], "properties": {
        "type": {"type": "string", "enum": ["npc_knowledge", "npc_goto", "npc_approach_player", "npc_fight_npc",
                                             "gather", "spawn_monster", "ambient_line", "quest_new"]},
        "npc": {"type": "string"}, "npc2": {"type": "string"}, "npcs": {"type": "array", "items": {"type": "string"}},
        "place": {"type": "string"}, "monster": {"type": "string"}, "fact": {"type": "string"}, "text": {"type": "string"}
      }}}
    }
  })JSON");
}

}  // namespace grog::ai
