add_library(lexer)

target_sources(lexer
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            token/token.cppm
            diagnostic/diagnostic.cppm
            charset/charset.cppm
            scanner/reporter.cppm
            scanner/keyword.cppm
            scanner/punctuation.cppm
            scanner/literal.cppm
            scanner/engine.cppm
            scanner/scanner.cppm
            lexer.cppm
)

target_link_libraries(lexer
    PUBLIC
        source
        preprocessor
)

set_target_properties(lexer PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
