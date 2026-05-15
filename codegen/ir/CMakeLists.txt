add_library(codegen_ir)

target_sources(codegen_ir
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            ir.cppm
)

target_link_libraries(codegen_ir
    PUBLIC
        semantic
        stdc++exp
)

set_target_properties(codegen_ir PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
