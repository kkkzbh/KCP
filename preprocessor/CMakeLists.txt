add_library(preprocessor_core)

target_sources(preprocessor_core
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            core.cppm
)

target_link_libraries(preprocessor_core
    PUBLIC
        lexer_support
)

set_target_properties(preprocessor_core PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
