// Plan 6 / Task 4: NPC lines are voiced (answers, fallbacks, cached farewells).
#include <doctest/doctest.h>

#include <grog/ai/bridge.h>
#include <grog/ai/hints.h>
#include <grog/ai/io.h>
#include <grog/ai/session.h>
#include <grog/ai/tts.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <sstream>

using namespace grog::ai;
using nlohmann::json;
namespace fs = std::filesystem;

namespace {
// Fresh, unique cache directory under the system temp dir, removed at the end.
struct TempDir {
  fs::path path;
  TempDir() {
    static std::atomic<int> n{0};
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path = fs::temp_directory_path() /
           pathFromUtf8("grog-voice-test-" + std::to_string(stamp) + "-" + std::to_string(n++));
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

struct VoiceFx {
  MockProvider actor, director;
  MockTts tts;
  std::ostringstream log;
  Telemetry tel;
  TempDir cache;
  std::unique_ptr<GameSession> s;
  explicit VoiceFx(bool voiceOn = true, Pricing p = Pricing{}, TtsVoices voices = TtsVoices::OpenAi)
      : tel(log, false, std::move(p)) {
    auto d = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
    REQUIRE(d.ok());
    AiConfig cfg;
    cfg.tts.kind = TtsKind::OpenAiCompatible;
    cfg.tts.apiKey = "sk-tts-SECRET";
    cfg.tts.cacheDir = pathToUtf8(cache.path);
    cfg.tts.voices = voices;
    s = std::make_unique<GameSession>(cfg, d.value(), actor, director, tel, 8);
    if(voiceOn)
      s->setTts(&tts);
  }
  std::vector<json> lines(const std::string& kind) const {
    std::vector<json> out;
    std::istringstream in(log.str());
    for(std::string l; std::getline(in, l);) {
      auto j = json::parse(l, nullptr, false);
      if(!j.is_discarded() && j.value("kind", "") == kind)
        out.push_back(j);
    }
    return out;
  }
};

bool isWav(const std::string& s) { return s.size() > 12 && s.compare(0, 4, "RIFF") == 0 && s.compare(8, 4, "WAVE") == 0; }
}  // namespace

TEST_CASE("voice: an answer comes with audio") {
  VoiceFx f;
  f.actor.scripted = {R"({"speech":"Czego chcesz?"})"};
  auto r = f.s->talk("greg", "Siema", 10);
  CHECK_FALSE(r.usedFallback);
  CHECK(isWav(r.audioWav));
  REQUIRE(f.tts.requests.size() == 1);
  const auto& card = f.s->data().cards.at("greg");
  CHECK(f.tts.requests[0].voice == card.voice.name);
  CHECK(f.tts.requests[0].voice == "onyx");
  CHECK(f.tts.requests[0].instructions == card.voice.style.at("pl"));
  CHECK(f.tts.requests[0].text == r.speech);
  auto tl = f.lines("tts");
  REQUIRE(tl.size() == 1);
  CHECK(tl[0]["npc"] == "greg");
  CHECK(tl[0]["ok"] == true);
  CHECK(tl[0]["cached"] == false);
  CHECK(tl[0]["audio_ms"] == 100);
  CHECK(tl[0]["chars"] == utf8Length(r.speech));
  CHECK(tl[0]["model"] == "mock");
  CHECK(f.log.str().find("sk-tts-SECRET") == std::string::npos);
}

TEST_CASE("voice: fallback lines are voiced too") {
  VoiceFx f;
  f.actor.scriptedErrors = {Error{ErrorKind::BadResponse, "junk"}};
  f.actor.scripted = {"not json", "still not json"};
  auto r = f.s->talk("greg", "Siema", 10);
  CHECK(r.usedFallback);
  CHECK(isWav(r.audioWav));
  REQUIRE(f.tts.requests.size() == 1);
  CHECK(f.tts.requests[0].text == r.speech);
}

TEST_CASE("voice: voice off does no tts work") {
  VoiceFx f(false);
  f.actor.scripted = {R"({"speech":"Czego chcesz?"})"};
  auto r = f.s->talk("greg", "Siema", 10);
  CHECK(r.audioWav.empty());
  CHECK(f.s->farewellAsync("greg").get().audioWav.empty());
  CHECK(f.s->speakAsync("greg", "Hej", true).get().audioWav.empty());
  CHECK(f.tts.requests.empty());
  CHECK(f.lines("tts").empty());
  CHECK_FALSE(fs::exists(f.cache.path));
  auto sum = f.s->summary();
  CHECK(sum["tts_requests"] == 0);
}

TEST_CASE("voice: tts failure keeps the text") {
  VoiceFx f;
  f.tts.scriptedErrors = {Error{ErrorKind::Auth, "bad key"}, Error{ErrorKind::Auth, "bad key"}};
  f.actor.scripted = {R"({"speech":"Czego chcesz?"})", R"({"speech":"No i co?"})"};
  auto a = f.s->talk("greg", "Siema", 10);
  CHECK(a.speech == "Czego chcesz?");
  CHECK_FALSE(a.usedFallback);
  CHECK(a.audioWav.empty());
  CHECK(a.hint == voiceHint(ErrorKind::Auth, Language::Pl));
  CHECK(a.hint.find("GrogAI.ini") != std::string::npos);
  CHECK(a.hint.find("[tts]") != std::string::npos);
  auto b = f.s->talk("greg", "Halo?", 20);
  CHECK(b.speech == "No i co?");
  CHECK(b.audioWav.empty());
  CHECK(b.hint.empty());
  auto tl = f.lines("tts");
  REQUIRE(tl.size() == 2);
  CHECK(tl[0]["ok"] == false);
  CHECK(tl[0]["error"] == "Auth");

  SUBCASE("LLM error hints keep priority when both fail in one turn") {
    VoiceFx g;
    g.actor.scriptedErrors = {Error{ErrorKind::Auth, "bad llm key"}};
    g.tts.scriptedErrors = {Error{ErrorKind::RateLimit, "quota"}, Error{ErrorKind::RateLimit, "quota"}};
    auto r = g.s->talk("greg", "Siema", 10);
    CHECK(r.usedFallback);
    CHECK(r.hint == playerHint(ErrorKind::Auth, Language::Pl));
    // The voice hint was not shown yet, so it comes the next time.
    g.actor.scripted = {R"({"speech":"Czego?"})"};
    auto n = g.s->talk("greg", "Halo", 20);
    CHECK(n.hint == voiceHint(ErrorKind::RateLimit, Language::Pl));
  }
}

TEST_CASE("voice: hints exist for voice errors in both languages") {
  for(ErrorKind k : {ErrorKind::Auth, ErrorKind::RateLimit, ErrorKind::Network, ErrorKind::Timeout,
                     ErrorKind::BadResponse})
    for(Language l : {Language::Pl, Language::En}) {
      std::string h = voiceHint(k, l);
      CHECK_FALSE(h.empty());
      CHECK(h.find("sk-") == std::string::npos);
    }
  CHECK(voiceHint(ErrorKind::Auth, Language::En).find("[tts]") != std::string::npos);
  CHECK(voiceHint(ErrorKind::BadResponse, Language::En).find("response_format=wav") != std::string::npos);
  CHECK(voiceHint(ErrorKind::BadResponse, Language::Pl).find("response_format=wav") != std::string::npos);
  CHECK(voiceHint(ErrorKind::Validation, Language::Pl).empty());
}

TEST_CASE("voice: farewells are cached") {
  VoiceFx f;
  // Greg's farewells differ; speak the same line twice through the farewell path until both match.
  Speech a = f.s->farewellAsync("greg").get();
  Speech b;
  for(int i = 0; i < 50; ++i) {
    b = f.s->farewellAsync("greg").get();
    if(b.text == a.text)
      break;
  }
  REQUIRE(b.text == a.text);
  CHECK(isWav(a.audioWav));
  CHECK(b.audioWav == a.audioWav);
  size_t requestsForLine = 0;
  for(auto& r : f.tts.requests)
    requestsForLine += r.text == a.text ? 1 : 0;
  CHECK(requestsForLine == 1);
  auto tl = f.lines("tts");
  REQUIRE(tl.size() >= 2);
  CHECK(tl[0]["cached"] == false);
  CHECK(tl.back()["cached"] == true);
  CHECK(tl.back()["cost_usd"] == 0.0);

  int files = 0;
  for(auto& e : fs::directory_iterator(f.cache.path)) {
    ++files;
    std::string name = pathToUtf8(e.path().filename());
    REQUIRE(name.size() == 16 + 4);
    CHECK(name.substr(16) == ".wav");
    CHECK(name.substr(0, 16).find_first_not_of("0123456789abcdef") == std::string::npos);
    auto content = readTextFile(e.path());
    REQUIRE(content.ok());
    CHECK(content.value().find("sk-tts-SECRET") == std::string::npos);
    CHECK(isWav(content.value()));
  }
  CHECK(files >= 1);
}

TEST_CASE("voice: a broken cache file counts as a miss") {
  VoiceFx f;
  Speech a = f.s->speakAsync("greg", "Spadaj.", true).get();
  REQUIRE(isWav(a.audioWav));
  for(auto& e : fs::directory_iterator(f.cache.path))
    REQUIRE(writeTextFile(e.path(), "garbage").ok());
  Speech b = f.s->speakAsync("greg", "Spadaj.", true).get();
  CHECK(isWav(b.audioWav));
  CHECK(f.tts.requests.size() == 2);
}

TEST_CASE("voice: answers are not cached") {
  VoiceFx f;
  f.actor.scripted = {R"({"speech":"Czego?"})", R"({"speech":"Czego?"})"};
  auto a = f.s->talk("greg", "Siema", 10);
  auto b = f.s->talk("greg", "Siema", 20);
  CHECK(a.speech == b.speech);
  CHECK(f.tts.requests.size() == 2);
  CHECK(f.s->speakAsync("greg", "Raz.", false).get().audioWav.size() > 0);
  CHECK(f.s->speakAsync("greg", "Raz.", false).get().audioWav.size() > 0);
  CHECK(f.tts.requests.size() == 4);
  CHECK_FALSE(fs::exists(f.cache.path));
}

TEST_CASE("voice: an npc without a voice uses the default") {
  VoiceFx f;
  auto sp = f.s->speakAsync("nobody", "Hej.", false).get();
  CHECK(isWav(sp.audioWav));
  REQUIRE(f.tts.requests.size() == 1);
  CHECK(f.tts.requests[0].voice == "ash");
  CHECK(f.tts.requests[0].instructions.empty());
}

TEST_CASE("voice: voices=original sends the npc id and no instructions") {
  VoiceFx orig(true, Pricing{}, TtsVoices::Original);
  CHECK(isWav(orig.s->speakAsync("greg", "Spadaj.", false).get().audioWav));
  REQUIRE(orig.tts.requests.size() == 1);
  CHECK(orig.tts.requests[0].voice == "greg");
  CHECK(orig.tts.requests[0].instructions.empty());
  CHECK(orig.tts.requests[0].text == "Spadaj.");

  VoiceFx def;
  CHECK(isWav(def.s->speakAsync("greg", "Spadaj.", false).get().audioWav));
  REQUIRE(def.tts.requests.size() == 1);
  CHECK(def.tts.requests[0].voice == "onyx");
  CHECK(def.tts.requests[0].instructions == def.s->data().cards.at("greg").voice.style.at("pl"));
}

TEST_CASE("voice: original-mode cache keys differ from openai-mode keys") {
  VoiceFx f;
  REQUIRE(isWav(f.s->speakAsync("greg", "Spadaj.", true).get().audioWav));
  // A second session in original mode shares the same cache directory.
  AiConfig cfg;
  cfg.tts.kind = TtsKind::OpenAiCompatible;
  cfg.tts.cacheDir = pathToUtf8(f.cache.path);
  cfg.tts.voices = TtsVoices::Original;
  MockTts tts2;
  GameSession s2(cfg, f.s->data(), f.actor, f.director, f.tel, 8);
  s2.setTts(&tts2);
  REQUIRE(isWav(s2.speakAsync("greg", "Spadaj.", true).get().audioWav));
  CHECK(tts2.requests.size() == 1);  // not served from the openai-mode file
  int files = 0;
  for(auto& e : fs::directory_iterator(f.cache.path)) {
    (void)e;
    ++files;
  }
  CHECK(files == 2);
}

namespace {
// TTS provider that blocks until released, so a load really happens while the audio is in flight.
struct GateTts : ITtsProvider {
  std::promise<void> entered, release;
  Result<TtsReply> synthesize(const TtsRequest& r) override {
    entered.set_value();
    release.get_future().wait();
    MockTts m;
    return m.synthesize(r);
  }
  std::string_view model() const override { return "gate"; }
};
}  // namespace

TEST_CASE("voice: late audio after loadState is dropped") {
  SUBCASE("talkAsync") {
    VoiceFx f(false);
    GateTts gate;
    f.s->setTts(&gate);
    json before = f.s->saveState();
    f.actor.scripted = {R"({"speech":"Za pozno.","actions":[{"type":"attitude","delta":2}]})"};
    auto fut = f.s->talkAsync("skip", "Hej", 1);
    gate.entered.get_future().wait();
    REQUIRE(f.s->loadState(before).ok());
    gate.release.set_value();
    auto r = fut.get();
    REQUIRE(r.error.has_value());
    CHECK(r.error->kind == ErrorKind::Validation);
    CHECK(r.audioWav.empty());
    CHECK(r.verdicts.empty());
    CHECK_FALSE(r.endDialog);
    CHECK(f.s->worldSnapshot().npcs.at("skip").attitude == 0);
  }
  SUBCASE("speakAsync") {
    VoiceFx f(false);
    GateTts gate;
    f.s->setTts(&gate);
    json before = f.s->saveState();
    auto fut = f.s->speakAsync("skip", "Ahoj!", false);
    gate.entered.get_future().wait();
    REQUIRE(f.s->loadState(before).ok());
    gate.release.set_value();
    auto sp = fut.get();
    CHECK(sp.text.empty());
    CHECK(sp.audioWav.empty());
    CHECK(sp.hint.empty());
  }
}

TEST_CASE("voice: summary counts tts") {
  Pricing p;
  p.ttsPerMinute["mock"] = 0.015;
  VoiceFx f(true, p);
  f.tts.scriptedErrors = {Error{ErrorKind::Network, "down"}};
  CHECK(f.s->speakAsync("greg", "Raz.", false).get().audioWav.empty());
  CHECK(isWav(f.s->speakAsync("greg", "Dwa.", false).get().audioWav));
  auto sum = f.s->summary();
  CHECK(sum["tts_requests"] == 2);
  CHECK(sum["tts_errors"] == 1);
  CHECK(sum["tts_audio_s"].get<double>() == doctest::Approx(0.1));
  CHECK(sum["tts_cost_usd"].get<double>() == doctest::Approx(0.015 * 100 / 60000.0));
  CHECK(sum.contains("tts_latency_avg_ms"));
  auto tl = f.lines("tts");
  REQUIRE(tl.size() == 2);
  CHECK(tl[1]["cost_usd"].get<double>() == doctest::Approx(0.015 * 100 / 60000.0));
  CHECK_FALSE(tl[1].contains("price_unknown"));

  // Saved and loaded with the rest of the stats.
  json saved = f.s->saveState();
  VoiceFx g;
  REQUIRE(g.s->loadState(saved).ok());
  CHECK(g.s->summary()["tts_requests"] == 2);
}

TEST_CASE("voice: an unknown tts model is logged with price_unknown") {
  VoiceFx f;
  CHECK(isWav(f.s->speakAsync("greg", "Raz.", false).get().audioWav));
  auto tl = f.lines("tts");
  REQUIRE(tl.size() == 1);
  CHECK(tl[0]["cost_usd"] == 0.0);
  CHECK(tl[0]["price_unknown"] == true);
}

TEST_CASE("voice: pricing.json has a tts price") {
  auto d = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
  REQUIRE(d.ok());
  REQUIRE(d.value().pricing.ttsPerMinute.count("gpt-4o-mini-tts"));
  CHECK(d.value().pricing.estimateTts("gpt-4o-mini-tts", 60000) ==
        doctest::Approx(d.value().pricing.ttsPerMinute.at("gpt-4o-mini-tts")));
  CHECK(d.value().pricing.estimateTts("unknown-tts", 60000) < 0);
  CHECK_FALSE(loadPricing(json{{"schema", 1}, {"tts", {{"x", {{"usd_per_minute", "free"}}}}}}).ok());
}

TEST_CASE("voice: bridge keeps utf-8 text") {
  auto d = loadGameData(pathFromUtf8(std::string(GROG_REPO_ROOT) + "/mod/data"));
  REQUIRE(d.ok());
  Verdict v{Command{"npc_approach_player", {{"npc", "bill"}, {"text", "Żółć"}}}, true, ""};
  auto acts = toEngineActions({v}, kDirector, d.value());
  REQUIRE(acts.size() == 1);
  CHECK(acts[0].textUtf8 == "Żółć");
  CHECK(acts[0].text == toGameText("Żółć"));
  CHECK(acts[0].text != acts[0].textUtf8);
}
