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
            type.cppm
            expression.cppm
            statement.cppm
            program.cppm
            parser.cppm
)

target_link_libraries(parser
    PUBLIC
        diagnostic
        source
        lexer
)

set_target_properties(parser PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
