add_library(parser)

target_sources(parser
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ast/ids.cppm
            ast/name.cppm
            ast/source.cppm
            ast/type.cppm
            ast/expr.cppm
            ast/stmt.cppm
            ast/item.cppm
            ast/arena.cppm
            ast/visit.cppm
            ast/ast.cppm
            diagnostic/diagnostic.cppm
            expression/precedence.cppm
            ll1/grammar.cppm
            op/trace.cppm
            op/operator_precedence.cppm
            syntax/result.cppm
            syntax/recursive_descent.cppm
            syntax/syntax.cppm
            parser.cppm
            syntax/recursive_descent/context.cppm
            syntax/recursive_descent/type.cppm
            syntax/recursive_descent/expression.cppm
            syntax/recursive_descent/statement.cppm
            syntax/recursive_descent/program.cppm
)

target_link_libraries(parser
    PUBLIC
        source
        lexer
)

set_target_properties(parser PROPERTIES
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
        parser
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
        parser
)

set_target_properties(op_precedence_demo PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
