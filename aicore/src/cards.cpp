#include <grog/ai/cards.h>

namespace grog::ai {
using nlohmann::json;

namespace {

bool readText(const json& j, const char* key, LangText& out, std::string& err) {
  if(!j.contains(key) || !j[key].is_object()) {
    err = std::string("missing '") + key + "'";
    return false;
  }
  for(auto [lang, code] : {std::pair{Language::Pl, "pl"}, std::pair{Language::En, "en"}}) {
    if(!j[key].contains(code) || !j[key][code].is_string() || j[key][code].get<std::string>().empty()) {
      err = std::string("missing '") + key + "." + code + "'";
      return false;
    }
    out[lang] = j[key][code].get<std::string>();
  }
  return true;
}

bool readLines(const json& j, const char* key, LangLines& out, bool required, std::string& err) {
  if(!j.contains(key)) {
    if(required)
      err = std::string("missing '") + key + "'";
    return !required;
  }
  for(auto [lang, code] : {std::pair{Language::Pl, "pl"}, std::pair{Language::En, "en"}}) {
    if(!j[key].contains(code) || !j[key][code].is_array() || j[key][code].empty()) {
      if(required) {
        err = std::string("missing '") + key + "." + code + "'";
        return false;
      }
      continue;
    }
    out[lang] = j[key][code].get<std::vector<std::string>>();
  }
  return true;
}

}  // namespace

Result<NpcCard> loadCard(const json& j) {
  NpcCard c;
  std::string err;
  if(j.value("schema", 0) != 1)
    return Error{ErrorKind::Io, "npc card: unsupported schema"};
  c.id = j.value("id", "");
  c.name = j.value("name", "");
  c.campRole = j.value("camp_role", "");
  if(c.id.empty() || c.name.empty())
    return Error{ErrorKind::Io, "npc card: id and name are required"};
  if(!readText(j, "personality", c.personality, err) || !readText(j, "speech_style", c.speechStyle, err) ||
     !readLines(j, "sample_lines", c.sampleLines, false, err) || !readLines(j, "farewells", c.farewells, true, err) ||
     !readLines(j, "fallback_lines", c.fallbackLines, true, err))
    return Error{ErrorKind::Io, "npc card " + c.id + ": " + err};
  c.knowsAbout = j.value("knows_about", std::vector<std::string>{});
  c.relations = j.value("relations", std::map<std::string, std::string>{});
  c.persuasion = j.value("persuasion", "");
  c.items = j.value("items", std::vector<std::string>{});
  return c;
}

Result<Lore> loadLore(const json& j) {
  if(j.value("schema", 0) != 1)
    return Error{ErrorKind::Io, "lore: unsupported schema"};
  Lore l;
  for(auto& e : j.value("entries", json::array()))
    l.entries.push_back({e.value("topic", ""), e.value("text", "")});
  return l;
}

}  // namespace grog::ai
