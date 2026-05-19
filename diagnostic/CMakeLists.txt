add_library(diagnostic)

target_sources(diagnostic
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            stage.cppm
            kind.cppm
            record.cppm
            catalog.cppm
            collector.cppm
            diagnostic.cppm
)

target_link_libraries(diagnostic
    PUBLIC
        source
)

set_target_properties(diagnostic PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
