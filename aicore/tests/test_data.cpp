#include <doctest/doctest.h>

#include <grog/ai/session.h>

#include <regex>

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
