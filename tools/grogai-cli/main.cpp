// grogai-cli: talk to the pirates in a terminal, using the real AI core (no game needed).
//
//   grogai-cli --data mod/data --config GrogAI.ini [--seed N] [--npc greg]
//
// Commands: /npc <id>  switch NPC   /npcs  list NPCs with cards   /director [trigger]  run the Game Master
//           /quests    quest log    /world  world state (JSON, spoilers!)   /bye  farewell   /quit
#include <grog/ai/config.h>
#include <grog/ai/io.h>
#include <grog/ai/llm.h>
#include <grog/ai/session.h>
#include <grog/ai/version.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace grog::ai;

namespace {

#ifdef _WIN32
std::string narrow(const std::wstring& w) {
  if(w.empty())
    return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(size_t(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}
#endif

// Reads one line as UTF-8 (the Windows console needs ReadConsoleW for Polish letters).
bool readLine(std::string& out) {
#ifdef _WIN32
  HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode = 0;
  if(GetConsoleMode(in, &mode)) {
    std::wstring w;
    wchar_t buf[512];
    for(;;) {
      DWORD read = 0;
      if(!ReadConsoleW(in, buf, DWORD(std::size(buf)), &read, nullptr) || read == 0)
        return !w.empty();
      w.append(buf, read);
      if(!w.empty() && w.back() == L'\n')
        break;
    }
    while(!w.empty() && (w.back() == L'\n' || w.back() == L'\r'))
      w.pop_back();
    out = narrow(w);
    return true;
  }
#endif
  if(!std::getline(std::cin, out))
    return false;
  if(!out.empty() && out.back() == '\r')
    out.pop_back();
  return true;
}

std::string arg(int argc, char** argv, const std::string& name, const std::string& def = "") {
  for(int i = 1; i + 1 < argc; ++i)
    if(argv[i] == name)
      return argv[i + 1];
  return def;
}

void printVerdicts(const std::vector<Verdict>& vs, bool debug) {
  for(auto& v : vs) {
    if(v.accepted && (v.command.type == "quest_entry" || v.command.type == "quest_new"))
      std::cout << "    [dziennik] " << v.command.args.value("text", "") << "\n";
    else if(v.accepted && v.command.type == "give_item")
      std::cout << "    [otrzymujesz] " << v.command.args.value("item", "") << "\n";
    else if(v.accepted && v.command.type == "start_fight")
      std::cout << "    [WALKA!]\n";
    else if(debug || !v.accepted)
      std::cout << "    (" << (v.accepted ? "ok" : "odrzucone") << ": " << v.command.type << " " << v.command.args.dump()
                << (v.reason.empty() ? "" : " - " + v.reason) << ")\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif
  std::cout << "grogai-cli " << version() << "\n";
  std::string dataDir = arg(argc, argv, "--data", "mod/data");
  std::string configPath = arg(argc, argv, "--config", "GrogAI.ini");
  std::string npc = arg(argc, argv, "--npc", "greg");

  auto ini = readTextFile(pathFromUtf8(configPath));
  if(!ini.ok()) {
    std::cerr << "Cannot read " << configPath << " (copy GrogAI.example.ini to GrogAI.ini)\n";
    return 2;
  }
  auto cfg = parseConfig(ini.value(), systemEnv());
  if(!cfg.ok()) {
    std::cerr << cfg.error().message << "\n";
    return 2;
  }
  auto data = loadGameData(pathFromUtf8(dataDir));
  if(!data.ok()) {
    std::cerr << data.error().message << "\n";
    return 2;
  }

  uint64_t seed = cfg.value().seed.value_or(
      static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count()));
  if(auto s = arg(argc, argv, "--seed"); !s.empty())
    seed = std::stoull(s);

  auto http = makeHttpClient();
  auto actor = makeProvider(cfg.value().actor, *http);
  auto director = makeProvider(cfg.value().director, *http);

  auto logPath = pathFromUtf8(cfg.value().logDir) / ("session-" + std::to_string(seed) + ".jsonl");
  std::error_code ec;
  std::filesystem::create_directories(logPath.parent_path(), ec);
  std::ofstream logFile(logPath, std::ios::binary | std::ios::app);
  Telemetry telemetry(logFile, true, data.value().pricing);

  GameSession session(cfg.value(), data.value(), *actor, *director, telemetry, seed);
  bool debug = cfg.value().debug;
  std::cout << redactedSummary(cfg.value()) << "\nseed=" << seed << "  log=" << pathToUtf8(logPath) << "\n"
            << "Rozmawiasz z: " << session.data().cast.displayName(npc) << "  (/npcs, /npc <id>, /director, /quests, "
            << "/bye, /quit)\n\n";

  auto start = std::chrono::steady_clock::now();
  auto gameTime = [&]() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count();
  };

  std::string line;
  for(;;) {
    std::cout << "Ty> " << std::flush;
    if(!readLine(line) || line == "/quit")
      break;
    if(line.empty())
      continue;
    if(line == "/npcs") {
      for(auto& [id, card] : session.data().cards)
        std::cout << "  " << id << " - " << card.name << "\n";
      continue;
    }
    if(line.rfind("/npc ", 0) == 0) {
      npc = line.substr(5);
      std::cout << "Rozmawiasz z: " << session.data().cast.displayName(npc) << "\n";
      continue;
    }
    if(line == "/quests") {
      for(auto& q : session.world().quests)
        std::cout << "  - " << q.text << "\n";
      continue;
    }
    if(line == "/world") {
      std::cout << toJson(session.world()).dump(2) << "\n";
      continue;
    }
    if(line == "/bye") {
      std::cout << session.data().cast.displayName(npc) << "> " << session.farewell(npc) << "\n";
      continue;
    }
    if(line.rfind("/director", 0) == 0) {
      std::string trig = line.size() > 10 ? line.substr(10) : "player_idle";
      auto r = session.direct(trig, gameTime());
      if(r.error)
        std::cout << "  [MG blad: " << errorKindName(r.error->kind) << " - " << r.error->message << "]\n";
      if(debug && !r.thought.empty())
        std::cout << "  [MG mysli: " << r.thought << "]\n";
      printVerdicts(r.verdicts, true);
      continue;
    }

    std::cout << "  ..." << std::flush;
    auto t0 = std::chrono::steady_clock::now();
    auto r = session.talkAsync(npc, line, gameTime()).get();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "\r" << session.data().cast.displayName(npc) << "> " << r.speech;
    if(debug)
      std::cout << "   [" << r.gesture << ", " << ms << " ms]";
    std::cout << "\n";
    if(r.error)
      std::cout << "    [blad AI: " << errorKindName(r.error->kind) << " - " << r.error->message << "]\n";
    printVerdicts(r.verdicts, debug);
    if(r.endDialog)
      std::cout << "    (" << session.data().cast.displayName(npc) << " konczy rozmowe)\n";
  }
  return 0;
}
