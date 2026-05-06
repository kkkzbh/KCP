add_library(source)

target_sources(source
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            source.cppm
)

set_target_properties(source PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
