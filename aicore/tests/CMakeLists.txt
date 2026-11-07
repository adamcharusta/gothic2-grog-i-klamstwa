FetchContent_Declare(doctest
  URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
  URL_HASH SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_GetProperties(doctest)
if(NOT doctest_POPULATED)
  FetchContent_Populate(doctest)
endif()

file(GLOB GROG_AICORE_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(aicore_tests ${GROG_AICORE_TEST_SOURCES})
target_include_directories(aicore_tests PRIVATE "${doctest_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(aicore_tests PRIVATE aicore)
target_compile_definitions(aicore_tests PRIVATE
  GROG_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/../.."
  GROG_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
if(MSVC)
  target_compile_options(aicore_tests PRIVATE /utf-8)
endif()
add_test(NAME aicore_tests COMMAND aicore_tests)
