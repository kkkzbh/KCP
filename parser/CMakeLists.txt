set(PARSER_SUPPORT_MODULES
    support/ast.cppm
    support/diagnostic.cppm
    support/trace.cppm
)

set(PARSER_CORE_MODULES
    core/grammar.cppm
    core/expression_precedence.cppm
    core/operator_precedence.cppm
    core/recursive_descent.cppm
    core/parser.cppm
)

add_library(parser_support)

target_sources(parser_support
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ${PARSER_SUPPORT_MODULES}
)

target_link_libraries(parser_support
    PUBLIC
        lexer_support
)

set_target_properties(parser_support PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_library(parser_core)

target_sources(parser_core
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ${PARSER_CORE_MODULES}
)

target_link_libraries(parser_core
    PUBLIC
        parser_support
        lexer_core
)

set_target_properties(parser_core PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_executable(parser_demo)

target_sources(parser_demo
    PRIVATE
        tool/main.cpp
)

target_link_libraries(parser_demo
    PRIVATE
        parser_core
)

set_target_properties(parser_demo PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_executable(op_precedence_demo)

target_sources(op_precedence_demo
    PRIVATE
        tool/op_precedence_demo.cpp
)

target_link_libraries(op_precedence_demo
    PRIVATE
        parser_core
)

set_target_properties(op_precedence_demo PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
