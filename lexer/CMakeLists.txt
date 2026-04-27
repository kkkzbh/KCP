set(LEXER_SUPPORT_MODULES
    support/source.cppm
    support/token.cppm
    support/diagnostic.cppm
    support/charset.cppm
    support/tables.cppm
)

set(LEXER_CORE_MODULES
    core/lexer.cppm
    core/scanner.cppm
)

add_library(lexer_support)

target_sources(lexer_support
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ${LEXER_SUPPORT_MODULES}
)

set_target_properties(lexer_support PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_library(lexer_core)

target_sources(lexer_core
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ${LEXER_CORE_MODULES}
)

target_link_libraries(lexer_core
    PUBLIC
        lexer_support
)

set_target_properties(lexer_core PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
