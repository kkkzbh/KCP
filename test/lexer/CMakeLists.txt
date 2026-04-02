set(LEXER_TEST_CASES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cases")
string(REPLACE "\\" "\\\\" LEXER_TEST_CASES_DIR_ESCAPED "${LEXER_TEST_CASES_DIR}")

function(add_lexer_suite target_name source_name test_name)
    add_executable(${target_name})

    target_sources(${target_name}
        PRIVATE
            ${source_name}
    )

    target_link_libraries(${target_name}
        PRIVATE
            lexer_core
    )

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/harness
    )

    target_compile_definitions(${target_name}
        PRIVATE
            TEST_LEXER_CASES_DIR="${LEXER_TEST_CASES_DIR_ESCAPED}"
    )

    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 26
        CXX_EXTENSIONS OFF
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON
    )

    add_test(NAME ${test_name} COMMAND ${target_name})
endfunction()

add_lexer_suite(lexer_valid_cases_test suites/valid_cases_test.cpp lexer_valid_cases)
add_lexer_suite(lexer_invalid_cases_test suites/invalid_cases_test.cpp lexer_invalid_cases)
add_lexer_suite(lexer_api_behavior_test suites/api_behavior_test.cpp lexer_api_behavior)
add_lexer_suite(lexer_support_behavior_test suites/support_behavior_test.cpp lexer_support_behavior)
