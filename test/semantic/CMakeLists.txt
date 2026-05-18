set(SEMANTIC_FIXTURE_EXAMPLES_DIR "${CMAKE_SOURCE_DIR}/test/fixtures/examples")
string(REPLACE "\\" "\\\\" SEMANTIC_FIXTURE_EXAMPLES_DIR_ESCAPED "${SEMANTIC_FIXTURE_EXAMPLES_DIR}")

function(add_semantic_suite target_name source_name test_name)
    add_executable(${target_name})

    target_sources(${target_name}
        PRIVATE
            ${source_name}
    )

    target_link_libraries(${target_name}
        PRIVATE
            semantic
            test_jsonl_support
            stdc++exp
    )

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/test/parser/harness
            ${CMAKE_SOURCE_DIR}/test/harness
    )

    target_compile_definitions(${target_name}
        PRIVATE
            TEST_SEMANTIC_FIXTURE_EXAMPLES_DIR="${SEMANTIC_FIXTURE_EXAMPLES_DIR_ESCAPED}"
    )

    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 26
        CXX_EXTENSIONS OFF
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON
    )

    add_test(NAME ${test_name} COMMAND ${target_name})
endfunction()

add_semantic_suite(semantic_behavior_test suites/behavior_test.cpp semantic_behavior)
