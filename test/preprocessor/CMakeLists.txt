set(PREPROCESSOR_TEST_CASES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cases")
string(REPLACE "\\" "\\\\" PREPROCESSOR_TEST_CASES_DIR_ESCAPED "${PREPROCESSOR_TEST_CASES_DIR}")

function(add_preprocessor_suite target_name source_name test_name)
    add_executable(${target_name})

    target_sources(${target_name}
        PRIVATE
            ${source_name}
    )

    target_link_libraries(${target_name}
        PRIVATE
            preprocessor
            test_jsonl_support
            nlohmann_json::nlohmann_json
    )

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/harness
            ${CMAKE_SOURCE_DIR}/test/harness
    )

    target_compile_definitions(${target_name}
        PRIVATE
            TEST_PREPROCESSOR_CASES_DIR="${PREPROCESSOR_TEST_CASES_DIR_ESCAPED}"
    )

    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 26
        CXX_EXTENSIONS OFF
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON
    )

    add_test(NAME ${test_name} COMMAND ${target_name})
endfunction()

add_preprocessor_suite(preprocessor_valid_cases_test suites/valid_cases_test.cpp preprocessor_valid_cases)
add_preprocessor_suite(preprocessor_invalid_cases_test suites/invalid_cases_test.cpp preprocessor_invalid_cases)
add_preprocessor_suite(preprocessor_api_behavior_test suites/api_behavior_test.cpp preprocessor_api_behavior)
