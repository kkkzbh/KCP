set(CP_LEXER_HELPER_ARTIFACT_DIR "${CMAKE_BINARY_DIR}/artifacts/native/linux-x86_64")

function(configure_cp_plugin_native_target target_name)
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 26
        CXX_EXTENSIONS OFF
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON
    )
endfunction()

add_library(cp_lexer_helper_lib)

target_sources(cp_lexer_helper_lib
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            src/api.cppm
)

target_link_libraries(cp_lexer_helper_lib
    PUBLIC
        lexer
        preprocessor
)

configure_cp_plugin_native_target(cp_lexer_helper_lib)

add_executable(cp_lexer_helper_cli)

target_sources(cp_lexer_helper_cli
    PRIVATE
        src/main.cpp
)

target_link_libraries(cp_lexer_helper_cli
    PRIVATE
        cp_lexer_helper_lib
)

set_target_properties(cp_lexer_helper_cli PROPERTIES
    OUTPUT_NAME "cp-lexer-helper"
    RUNTIME_OUTPUT_DIRECTORY "${CP_LEXER_HELPER_ARTIFACT_DIR}"
)

configure_cp_plugin_native_target(cp_lexer_helper_cli)

if(BUILD_TESTING)
    add_executable(cp_lexer_helper_api_behavior_test)

    target_sources(cp_lexer_helper_api_behavior_test
        PRIVATE
            tests/api_behavior_test.cpp
    )

    target_link_libraries(cp_lexer_helper_api_behavior_test
        PRIVATE
            cp_lexer_helper_lib
    )

    configure_cp_plugin_native_target(cp_lexer_helper_api_behavior_test)

    add_test(NAME cp_lexer_helper_api_behavior COMMAND cp_lexer_helper_api_behavior_test)
endif()
