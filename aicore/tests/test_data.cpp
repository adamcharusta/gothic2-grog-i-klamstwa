#include <doctest/doctest.h>

#include <grog/ai/session.h>

#include <regex>
#include <set>

using namespace grog::ai;

namespace {
GameData data() {
  auto d = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
  REQUIRE(d.ok());
  return d.value();
}
}  // namespace

TEST_CASE("every cast member has a card and an engine instance") {
  GameData d = data();
  std::regex inst(R"(^PIR_[0-9]+_ADDON_[A-Z_]+$)");
  for(auto& m : d.cast.npcs) {
    CAPTURE(m.id);
    CHECK(d.cards.count(m.id));
    CHECK(std::regex_match(m.instance, inst));
  }
}

TEST_CASE("every camp npc has a voice") {
  GameData d = data();
  const std::set<std::string> allowed{"ash", "ballad", "echo", "fable", "onyx", "verse"};
  auto endsWith = [](const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
  };
  auto codePoints = [](const std::string& s) {
    size_t n = 0;
    for(unsigned char c : s)
      n += (c & 0xC0) != 0x80;
    return n;
  };
  CHECK(d.cards.size() == 16);
  for(auto& [id, c] : d.cards) {
    CAPTURE(id);
    CHECK(allowed.count(c.voice.name));
    for(const char* lang : {"pl", "en"}) {
      CAPTURE(lang);
      REQUIRE(c.voice.style.count(lang));
      CHECK_FALSE(c.voice.style.at(lang).empty());
      CHECK(codePoints(c.voice.style.at(lang)) <= 200);
    }
    CHECK(endsWith(c.voice.style["pl"], "Mów po polsku."));
    CHECK(endsWith(c.voice.style["en"], "Speak English."));
  }
  CHECK(d.cards.at("greg").voice.name == "onyx");
  for(auto [a, b] : {std::pair{"angus", "hank"}, std::pair{"owen", "malcolm"}, std::pair{"greg", "francis"}}) {
    CAPTURE(a);
    CHECK(d.cards.at(a).voice.name != d.cards.at(b).voice.name);
  }
}

TEST_CASE("every place has a waypoint") {
  GameData d = data();
  for(auto& p : d.cast.places) {
    CAPTURE(p);
    REQUIRE(d.cast.placeWaypoints.count(p));
    CHECK(d.cast.placeWaypoints.at(p).rfind("ADW_", 0) == 0);
  }
}

TEST_CASE("story items and the start setup map to engine instances") {
  GameData d = data();
  for(const char* id : {"greg_tankard", "tankard_lookalike", "key_reward_chest", "captain_outfit"}) {
    CAPTURE(id);
    REQUIRE(d.items.count(id));
    CHECK_FALSE(d.items.at(id).empty());
  }
  CHECK(d.start.world == "ADDONWORLD.ZEN");
  CHECK(d.start.waypoint.rfind("ADW_PIRATECAMP", 0) == 0);
  CHECK(d.start.armor == "ITAR_PIR_L_ADDON");
  CHECK(d.start.level >= 5);
}

TEST_CASE("card items exist in items.json") {
  GameData d = data();
  for(auto& [id, card] : d.cards)
    for(auto& item : card.items) {
      CAPTURE(id);
      CAPTURE(item);
      CHECK(d.items.count(item));
    }
}

TEST_CASE("playtest 2: start.json has a first quest entry in both languages") {
  GameData d = data();
  CHECK(d.start.firstQuest.count("pl"));
  CHECK(d.start.firstQuest.count("en"));
  CHECK(d.start.firstQuest["pl"].find("Greg") != std::string::npos);
}
