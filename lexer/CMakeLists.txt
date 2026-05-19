add_library(lexer)

target_sources(lexer
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            token.cppm
            charset.cppm
            cursor.cppm
            state.cppm
            keyword.cppm
            punctuation.cppm
            literal.cppm
            lexer.cppm
)

target_link_libraries(lexer
    PUBLIC
        diagnostic
        source
        preprocessor
)

set_target_properties(lexer PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
