find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_library(codegen_llvm)

target_sources(codegen_llvm
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            llvm.cppm
)

target_include_directories(codegen_llvm
    SYSTEM
    PUBLIC
        ${LLVM_INCLUDE_DIRS}
)

separate_arguments(LLVM_DEFINITIONS_LIST UNIX_COMMAND "${LLVM_DEFINITIONS}")

target_compile_options(codegen_llvm
    PUBLIC
        ${LLVM_DEFINITIONS_LIST}
)

llvm_map_components_to_libnames(LLVM_LIBS
    core
    support
)

target_link_libraries(codegen_llvm
    PUBLIC
        semantic
        ${LLVM_LIBS}
)

set_target_properties(codegen_llvm PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)
