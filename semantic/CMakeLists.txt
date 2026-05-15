add_library(semantic)

target_sources(semantic
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            type/builtin.cppm
            type/ids.cppm
            type/nodes.cppm
            type/arena.cppm
            type/type.cppm
            symbol.cppm
            result.cppm
            modules.cppm
            types.cppm
            expressions.cppm
            statements.cppm
            return_infer.cppm
            semantic.cppm
)

target_link_libraries(semantic
    PUBLIC
        diagnostic
        parser
        stdc++exp
)

set_target_properties(semantic PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
