add_library(semantic)

target_sources(semantic
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ty/builtin.cppm
            ty/ids.cppm
            ty/nodes.cppm
            ty/arena.cppm
            ty/type.cppm
            diagnostic/diagnostic.cppm
            symbol/symbol.cppm
            result/keys.cppm
            result/result.cppm
            check/concepts.cppm
            check/keys.cppm
            check/state.cppm
            check/context.cppm
            check/types.cppm
            check/expressions.cppm
            check/statements.cppm
            check/analyzer.cppm
            check/check.cppm
            semantic.cppm
)

target_link_libraries(semantic
    PUBLIC
        parser
        stdc++exp
)

set_target_properties(semantic PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
