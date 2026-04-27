add_library(test_jsonl_support STATIC)

target_sources(test_jsonl_support
    PRIVATE
        jsonl.cpp
)

target_link_libraries(test_jsonl_support
    PRIVATE
        nlohmann_json::nlohmann_json
)

target_include_directories(test_jsonl_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(test_jsonl_support PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD OFF
    CXX_SCAN_FOR_MODULES OFF
)
