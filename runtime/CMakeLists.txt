add_library(cp_runtime STATIC)

target_sources(cp_runtime
    PRIVATE
        cp_runtime.cpp
)

set_target_properties(cp_runtime PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
)
