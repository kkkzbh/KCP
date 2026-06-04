add_library(cp_import_resolver)

target_sources(cp_import_resolver
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            import_resolver.cppm
)

target_link_libraries(cp_import_resolver
    PUBLIC
        preprocessor
        lexer
        parser
)

set_target_properties(cp_import_resolver PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_executable(kcp)

target_sources(kcp
    PRIVATE
        tool/main.cpp
)

target_link_libraries(kcp
    PRIVATE
        cp_import_resolver
        diagnostic
        preprocessor
        lexer
        codegen_ir
        codegen_llvm
        stdc++exp
)

add_dependencies(kcp cp_runtime)

target_compile_definitions(kcp
    PRIVATE
        CP_RUNTIME_LIBRARY_PATH="$<TARGET_FILE:cp_runtime>"
        CP_STDLIB_ROOT_PATH="${CMAKE_SOURCE_DIR}/std"
)

set_target_properties(kcp PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
