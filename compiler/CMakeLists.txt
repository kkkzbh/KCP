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

add_dependencies(cp cp_runtime)

target_compile_definitions(cp
    PRIVATE
        CP_RUNTIME_LIBRARY_PATH="$<TARGET_FILE:cp_runtime>"
        CP_STDLIB_ROOT_PATH="${CMAKE_SOURCE_DIR}/std"
)

set_target_properties(cp PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
