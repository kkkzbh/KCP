add_executable(lab_lexer_behavior_test)
add_executable(lab_parser_behavior_test)

target_sources(lab_lexer_behavior_test
    PRIVATE
        suites/lexer_behavior_test.cpp
)

target_sources(lab_parser_behavior_test
    PRIVATE
        suites/parser_behavior_test.cpp
)

target_link_libraries(lab_lexer_behavior_test
    PRIVATE
        stdc++exp
)

target_link_libraries(lab_parser_behavior_test
    PRIVATE
        stdc++exp
)

target_include_directories(lab_lexer_behavior_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/test/parser/harness
)

target_include_directories(lab_parser_behavior_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/test/parser/harness
)

set_target_properties(lab_lexer_behavior_test PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

set_target_properties(lab_parser_behavior_test PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_dependencies(lab_lexer_behavior_test cp)
add_dependencies(lab_parser_behavior_test cp)

add_test(
    NAME lab_lexer_behavior
    COMMAND lab_lexer_behavior_test $<TARGET_FILE:cp> ${CMAKE_SOURCE_DIR}/lab
)

add_test(
    NAME lab_parser_behavior
    COMMAND lab_parser_behavior_test $<TARGET_FILE:cp> ${CMAKE_SOURCE_DIR}/lab
)

set_tests_properties(lab_lexer_behavior PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(lab_parser_behavior PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
