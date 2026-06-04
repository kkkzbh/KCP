find_program(CP_TEST_CLANG clang REQUIRED)
find_program(CP_TEST_LLVM_AS llvm-as REQUIRED)

add_executable(compiler_behavior_test)
add_executable(compiler_import_resolver_test)

target_sources(compiler_behavior_test
    PRIVATE
        suites/behavior_test.cpp
)

target_sources(compiler_import_resolver_test
    PRIVATE
        suites/import_resolver_test.cpp
)

target_link_libraries(compiler_behavior_test
    PRIVATE
        test_jsonl_support
        stdc++exp
)

target_link_libraries(compiler_import_resolver_test
    PRIVATE
        cp_import_resolver
        test_jsonl_support
        stdc++exp
)

target_include_directories(compiler_behavior_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/test/parser/harness
        ${CMAKE_SOURCE_DIR}/test/harness
)

target_include_directories(compiler_import_resolver_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/test/parser/harness
        ${CMAKE_SOURCE_DIR}/test/harness
)

set_target_properties(compiler_behavior_test PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

set_target_properties(compiler_import_resolver_test PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_dependencies(compiler_behavior_test kcp)

add_test(
    NAME compiler_behavior
    COMMAND ${CMAKE_SOURCE_DIR}/test/run_isolated.sh
        $<TARGET_FILE:compiler_behavior_test>
        $<TARGET_FILE:kcp>
        ${CP_TEST_CLANG}
        ${CP_TEST_LLVM_AS}
        ${CMAKE_SOURCE_DIR}/test/fixtures/examples
)

add_test(
    NAME compiler_import_resolver
    COMMAND ${CMAKE_SOURCE_DIR}/test/run_isolated.sh
        $<TARGET_FILE:compiler_import_resolver_test>
)

set_tests_properties(compiler_behavior PROPERTIES
    RUN_SERIAL TRUE
)
