find_program(CP_TEST_CLANG clang REQUIRED)
find_program(CP_TEST_LLVM_AS llvm-as REQUIRED)

add_executable(compiler_behavior_test)

target_sources(compiler_behavior_test
    PRIVATE
        suites/behavior_test.cpp
)

target_link_libraries(compiler_behavior_test
    PRIVATE
        test_jsonl_support
        stdc++exp
)

target_include_directories(compiler_behavior_test
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

add_dependencies(compiler_behavior_test cp)

add_test(
    NAME compiler_behavior
    COMMAND compiler_behavior_test $<TARGET_FILE:cp> ${CP_TEST_CLANG} ${CP_TEST_LLVM_AS} ${CMAKE_SOURCE_DIR}/design/examples
)
