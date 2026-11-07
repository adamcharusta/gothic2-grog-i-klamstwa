#include <doctest/doctest.h>

#include <grog/ai/io.h>

#include <filesystem>

using namespace grog::ai;

TEST_CASE("text files round-trip under a path with Polish characters") {
  auto dir = std::filesystem::temp_directory_path() / pathFromUtf8("grog ł – test");
  auto file = dir / pathFromUtf8("żółć.txt");
  REQUIRE(writeTextFile(file, "Kufel Grega: ąęłńóśźż").ok());
  auto r = readTextFile(file);
  REQUIRE(r.ok());
  CHECK(r.value() == "Kufel Grega: ąęłńóśźż");
  CHECK(pathToUtf8(file).find("żółć.txt") != std::string::npos);
  std::filesystem::remove_all(dir);
}

TEST_CASE("reading a missing file is an Io error") {
  auto r = readTextFile(pathFromUtf8("does/not/exist.txt"));
  REQUIRE_FALSE(r.ok());
  CHECK(r.error().kind == ErrorKind::Io);
}
