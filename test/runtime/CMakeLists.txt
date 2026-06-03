add_executable(runtime_api_test)

target_sources(runtime_api_test
    PRIVATE
        suites/runtime_api_test.cpp
)

target_link_libraries(runtime_api_test
    PRIVATE
        cp_runtime
        stdc++exp
)

target_include_directories(runtime_api_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/test/parser/harness
)

set_target_properties(runtime_api_test PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_test(NAME runtime_api COMMAND runtime_api_test)
