add_library(preprocessor)

target_sources(preprocessor
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            diagnostic.cppm
            preprocessed.cppm
            preprocessor.cppm
)

target_link_libraries(preprocessor
    PUBLIC
        source
)

set_target_properties(preprocessor PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
