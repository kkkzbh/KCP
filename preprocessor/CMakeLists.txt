add_library(preprocessor)

target_sources(preprocessor
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            diagnostic/diagnostic.cppm
            preprocessed/preprocessed.cppm
            scanner/scanner.cppm
            preprocessor.cppm
)

target_link_libraries(preprocessor
    PUBLIC
        lexer_support
)

set_target_properties(preprocessor PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
