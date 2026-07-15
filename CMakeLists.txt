cmake_minimum_required(VERSION 4.2.2)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.3)
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
else()
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
endif()
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

project(KCP LANGUAGES C CXX)

include(FetchContent)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

include(CTest)
add_subdirectory(source)
add_subdirectory(diagnostic)
add_subdirectory(preprocessor)
add_subdirectory(lexer)
add_subdirectory(parser)
add_subdirectory(semantic)
add_subdirectory(runtime)
add_subdirectory(codegen/ir)
add_subdirectory(codegen/llvm)
add_subdirectory(compiler)
add_subdirectory(test/module)
add_subdirectory(test/harness)
add_subdirectory(test/diagnostic)
add_subdirectory(test/lexer)
add_subdirectory(test/preprocessor)
add_subdirectory(test/parser)
add_subdirectory(test/semantic)
add_subdirectory(test/codegen)
add_subdirectory(test/compiler)
add_subdirectory(test/runtime)
add_subdirectory(test/benchmark)
add_subdirectory(clion-plugin/native)

install(DIRECTORY std/
    DESTINATION lib/kcp/std
    FILES_MATCHING
        PATTERN "*.cp"
)
