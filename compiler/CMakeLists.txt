add_executable(cp)

target_sources(cp
    PRIVATE
        tool/main.cpp
)

target_link_libraries(cp
    PRIVATE
        diagnostic
        preprocessor
        lexer
        codegen_ir
        codegen_llvm
        stdc++exp
)

set_target_properties(cp PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
