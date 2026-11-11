#include <grog/ai/bridge.h>

#include <algorithm>
#include <cctype>
#include <map>

namespace grog::ai {
using nlohmann::json;

namespace {

// Unicode code point for cp1250 bytes 0x80..0xFF (0 = undefined). Generated from Python's cp1250 codec.
const uint16_t kCp1250[128] = {0x20AC, 0x0000, 0x201A, 0x0000, 0x201E, 0x2026, 0x2020, 0x2021, 0x0000, 0x2030, 0x0160, 0x2039, 0x015A, 0x0164, 0x017D, 0x0179, 0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x0000, 0x2122, 0x0161, 0x203A, 0x015B, 0x0165, 0x017E, 0x017A, 0x00A0, 0x02C7, 0x02D8, 0x0141, 0x00A4, 0x0104, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x015E, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x017B, 0x00B0, 0x00B1, 0x02DB, 0x0142, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x0105, 0x015F, 0x00BB, 0x013D, 0x02DD, 0x013E, 0x017C, 0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7, 0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E, 0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7, 0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF, 0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7, 0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F, 0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7, 0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9};

void appendUtf8(std::string& out, uint32_t cp) {
  if(cp < 0x80) {
    out += char(cp);
  } else if(cp < 0x800) {
    out += char(0xC0 | (cp >> 6));
    out += char(0x80 | (cp & 0x3F));
  } else if(cp < 0x10000) {
    out += char(0xE0 | (cp >> 12));
    out += char(0x80 | ((cp >> 6) & 0x3F));
    out += char(0x80 | (cp & 0x3F));
  } else {
    out += char(0xF0 | (cp >> 18));
    out += char(0x80 | ((cp >> 12) & 0x3F));
    out += char(0x80 | ((cp >> 6) & 0x3F));
    out += char(0x80 | (cp & 0x3F));
  }
}

// Decodes one code point; returns false (and consumes one byte) on invalid input.
bool nextCodePoint(std::string_view s, size_t& i, uint32_t& cp) {
  unsigned char c = static_cast<unsigned char>(s[i]);
  int len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
  if(len == 0 || i + len > s.size()) {
    ++i;
    return false;
  }
  cp = len == 1 ? c : len == 2 ? (c & 0x1F) : len == 3 ? (c & 0x0F) : (c & 0x07);
  for(int k = 1; k < len; ++k) {
    unsigned char cc = static_cast<unsigned char>(s[i + k]);
    if((cc & 0xC0) != 0x80) {
      ++i;
      return false;
    }
    cp = (cp << 6) | (cc & 0x3F);
  }
  i += len;
  return true;
}

std::string upper(std::string_view s) {
  std::string u(s);
  std::transform(u.begin(), u.end(), u.begin(), [](unsigned char c) { return char(std::toupper(c)); });
  return u;
}

std::string str(const json& a, const char* k) {
  return a.contains(k) && a[k].is_string() ? a[k].get<std::string>() : std::string();
}

}  // namespace

std::string toGameText(std::string_view utf8) {
  static const std::map<uint32_t, const char*> kAscii = {
      {0x201E, "\""}, {0x201C, "\""}, {0x201D, "\""}, {0x00AB, "\""}, {0x00BB, "\""}, {0x2018, "'"}, {0x2019, "'"},
      {0x201A, "'"},  {0x2013, "-"},  {0x2014, "-"},  {0x2026, "..."}, {0x00A0, " "}};
  static const std::map<uint32_t, unsigned char> kReverse = [] {
    std::map<uint32_t, unsigned char> m;
    for(int b = 0; b < 128; ++b)
      if(kCp1250[b] != 0)
        m[kCp1250[b]] = static_cast<unsigned char>(0x80 + b);
    return m;
  }();
  std::string out;
  out.reserve(utf8.size());
  for(size_t i = 0; i < utf8.size();) {
    uint32_t cp = 0;
    if(!nextCodePoint(utf8, i, cp)) {
      out += '?';
      continue;
    }
    if(cp < 0x80) {
      out += char(cp);
    } else if(auto a = kAscii.find(cp); a != kAscii.end()) {
      out += a->second;
    } else if(auto r = kReverse.find(cp); r != kReverse.end()) {
      out += char(r->second);
    } else {
      out += '?';
    }
  }
  return out;
}

std::string fromGameText(std::string_view cp1250) {
  std::string out;
  out.reserve(cp1250.size());
  for(unsigned char c : cp1250) {
    if(c < 0x80)
      out += char(c);
    else if(kCp1250[c - 0x80] != 0)
      appendUtf8(out, kCp1250[c - 0x80]);
    else
      out += '?';
  }
  return out;
}

std::optional<std::string> npcIdForInstance(const GameData& data, std::string_view instance) {
  std::string u = upper(instance);
  for(auto& m : data.cast.npcs)
    if(upper(m.instance) == u)
      return m.id;
  return std::nullopt;
}

std::string instanceForNpc(const GameData& data, std::string_view npcId) {
  for(auto& m : data.cast.npcs)
    if(m.id == npcId)
      return m.instance;
  return {};
}

std::vector<EngineAction> toEngineActions(const std::vector<Verdict>& verdicts, std::string_view speaker,
                                          const GameData& data) {
  std::vector<EngineAction> out;
  auto wp = [&](const std::string& place) {
    auto it = data.cast.placeWaypoints.find(place);
    return it == data.cast.placeWaypoints.end() ? std::string() : it->second;
  };
  std::string speakerInst = instanceForNpc(data, speaker);
  for(auto& v : verdicts) {
    if(!v.accepted)
      continue;
    const json& a = v.command.args;
    const std::string& t = v.command.type;
    EngineAction e;
    if(t == "quest_entry" || t == "quest_new") {
      e.kind = EngineAction::QuestEntry;
      e.textUtf8 = str(a, "text");
      e.text = toGameText(e.textUtf8);
    } else if(t == "give_item") {
      auto it = data.items.find(str(a, "item"));
      if(it == data.items.end() || speakerInst.empty())
        continue;
      e.kind = EngineAction::GiveItemToPlayer;
      e.npcInstance = speakerInst;
      e.itemInstance = it->second;
    } else if(t == "take_item") {
      auto it = data.items.find(str(a, "item"));
      if(it == data.items.end() || speakerInst.empty())
        continue;
      e.kind = EngineAction::TakeItemFromPlayer;
      e.npcInstance = speakerInst;
      e.itemInstance = it->second;
    } else if(t == "end_game") {
      std::string o = str(a, "outcome");
      if((o == "returned" || o == "fooled") && data.items.count(kRewardKey) && !speakerInst.empty())
        out.push_back(EngineAction{EngineAction::GiveItemToPlayer, speakerInst, "", data.items.at(kRewardKey), "", "", ""});
      e.kind = EngineAction::EndGame;
      e.npcInstance = speakerInst;
      e.text = o;
      e.textUtf8 = o;
    } else if(t == "start_fight") {
      if(speakerInst.empty())
        continue;
      e.kind = EngineAction::StartFight;
      e.npcInstance = speakerInst;
    } else if(t == "npc_goto") {
      e.kind = EngineAction::NpcGoto;
      e.npcInstance = instanceForNpc(data, str(a, "npc"));
      e.waypoint = wp(str(a, "place"));
    } else if(t == "npc_approach_player") {
      e.kind = EngineAction::NpcApproachPlayer;
      e.npcInstance = instanceForNpc(data, str(a, "npc"));
      e.textUtf8 = str(a, "text");  // the NPC's opening line
      e.text = toGameText(e.textUtf8);
    } else if(t == "npc_fight_npc") {
      e.kind = EngineAction::NpcFightNpc;
      e.npcInstance = instanceForNpc(data, str(a, "npc"));
      e.npc2Instance = instanceForNpc(data, str(a, "npc2"));
    } else if(t == "spawn_monster") {
      e.kind = EngineAction::SpawnMonster;
      e.monsterInstance = upper(str(a, "monster"));
      e.waypoint = wp(str(a, "place"));
    } else if(t == "ambient_line") {
      e.kind = EngineAction::AmbientLine;
      e.npcInstance = instanceForNpc(data, str(a, "npc"));
      e.textUtf8 = str(a, "text");
      e.text = toGameText(e.textUtf8);
    } else {
      continue;  // attitude, npc_knowledge: world-state only
    }
    if((e.kind != EngineAction::QuestEntry && e.kind != EngineAction::SpawnMonster && e.npcInstance.empty()) ||
       ((e.kind == EngineAction::NpcGoto || e.kind == EngineAction::SpawnMonster) && e.waypoint.empty()))
      continue;
    out.push_back(std::move(e));
  }
  return out;
}

}  // namespace grog::ai
