cmake_minimum_required(VERSION 3.31)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

project(language_lab LANGUAGES CXX)

include(FetchContent)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

include(CTest)
add_subdirectory(lexer)
add_subdirectory(preprocessor)
target_link_libraries(lexer_core PRIVATE preprocessor)
add_subdirectory(parser)
add_subdirectory(test/module)
add_subdirectory(test/harness)
add_subdirectory(test/lexer)
add_subdirectory(test/preprocessor)
add_subdirectory(test/parser)
add_subdirectory(clion-plugin/native)
