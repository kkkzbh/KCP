add_executable(cp_test_module)

target_sources(cp_test_module
    PRIVATE
        main.cpp
    PRIVATE
        FILE_SET CXX_MODULES
        FILES
            smoke.cppm
)

set_target_properties(cp_test_module PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_test(NAME module COMMAND cp_test_module)
