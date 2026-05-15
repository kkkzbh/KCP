add_executable(diagnostic_behavior_test)

target_sources(diagnostic_behavior_test
    PRIVATE
        suites/behavior_test.cpp
)

target_link_libraries(diagnostic_behavior_test
    PRIVATE
        diagnostic
)

set_target_properties(diagnostic_behavior_test PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_test(NAME diagnostic_behavior COMMAND diagnostic_behavior_test)
