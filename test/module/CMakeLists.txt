add_executable(module_smoke_test)

target_sources(module_smoke_test
    PRIVATE
        main.cpp
    PRIVATE
        FILE_SET CXX_MODULES
        FILES
            smoke.cppm
)

set_target_properties(module_smoke_test PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_test(NAME module COMMAND module_smoke_test)
