cmake_minimum_required(VERSION 3.16)
project(GrogAiCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GROG_AICORE_TESTS "Build aicore unit tests" ${PROJECT_IS_TOP_LEVEL})
option(GROG_AICORE_CLI "Build grogai-cli" ${PROJECT_IS_TOP_LEVEL})

include(FetchContent)
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

# Version: VERSION file in the repo root, unless overridden.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" _grog_version)
string(STRIP "${_grog_version}" _grog_version)
set(GROG_AICORE_VERSION "${_grog_version}" CACHE STRING "Version string compiled into aicore")

file(GLOB GROG_AICORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
if(WIN32)
  list(FILTER GROG_AICORE_SOURCES EXCLUDE REGEX ".*/http_stub\\.cpp$")
else()
  list(FILTER GROG_AICORE_SOURCES EXCLUDE REGEX ".*/http_winhttp\\.cpp$")
endif()

add_library(aicore STATIC ${GROG_AICORE_SOURCES})
target_include_directories(aicore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(aicore PUBLIC nlohmann_json::nlohmann_json)
target_compile_definitions(aicore PRIVATE GROG_AICORE_VERSION="${GROG_AICORE_VERSION}")
if(WIN32)
  target_link_libraries(aicore PRIVATE winhttp)
  target_compile_definitions(aicore PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(MSVC)
  target_compile_options(aicore PRIVATE /W4 /utf-8)
else()
  target_compile_options(aicore PRIVATE -Wall -Wextra)
endif()

if(GROG_AICORE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(GROG_AICORE_CLI)
  add_subdirectory(../tools/grogai-cli "${CMAKE_CURRENT_BINARY_DIR}/grogai-cli")
endif()
