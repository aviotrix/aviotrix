include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1)
FetchContent_MakeAvailable(Catch2)

add_executable(aviotrix_core_tests
  test_status.cc
  test_avio.cc
  test_metadata.cc
  test_json.cc
  test_remux.cc)
target_link_libraries(aviotrix_core_tests PRIVATE aviotrix_core Catch2::Catch2WithMain)
target_compile_definitions(aviotrix_core_tests PRIVATE AVIOTRIX_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../fixtures")
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(aviotrix_core_tests)
