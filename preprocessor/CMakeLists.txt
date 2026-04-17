set(PREPROCESSOR_SUPPORT_MODULES
    support/issue.cppm
    support/output.cppm
)

set(PREPROCESSOR_CORE_MODULES
    core/preprocessor.cppm
    core/scanner.cppm
)

add_library(preprocessor_support)

target_sources(preprocessor_support
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ${PREPROCESSOR_SUPPORT_MODULES}
)

target_link_libraries(preprocessor_support
    PUBLIC
        lexer_support
)

set_target_properties(preprocessor_support PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_library(preprocessor_core)

target_sources(preprocessor_core
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ${PREPROCESSOR_CORE_MODULES}
)

target_link_libraries(preprocessor_core
    PUBLIC
        preprocessor_support
        lexer_support
)

set_target_properties(preprocessor_core PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
