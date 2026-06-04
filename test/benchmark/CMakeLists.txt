find_program(CP_BENCHMARK_CXX NAMES clang++ g++)
find_program(CP_BENCHMARK_RUSTC NAMES rustc)

add_executable(kcp_benchmark_runner)

target_sources(kcp_benchmark_runner
    PRIVATE
        runner.cpp
)

target_link_libraries(kcp_benchmark_runner
    PRIVATE
        stdc++exp
)

set_target_properties(kcp_benchmark_runner PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_dependencies(kcp_benchmark_runner kcp)

set(CP_BENCHMARK_SMOKE_CASES
    std_vector_string
    std_map_set
    std_sort_int_small
    std_sort_record_small
    std_sort_string_view_small
    std_sort_string_small
)

add_test(
    NAME kcp_benchmark
    COMMAND ${CMAKE_SOURCE_DIR}/test/run_isolated.sh
        $<TARGET_FILE:kcp_benchmark_runner>
        $<TARGET_FILE:kcp>
        "${CP_BENCHMARK_CXX}"
        "${CP_BENCHMARK_RUSTC}"
        ${CMAKE_CURRENT_SOURCE_DIR}/cases
        ${CP_BENCHMARK_SMOKE_CASES}
)

set_tests_properties(kcp_benchmark PROPERTIES
    LABELS benchmark
    RUN_SERIAL TRUE
    TIMEOUT 180
)

add_custom_target(benchmark
    COMMAND ${CMAKE_CTEST_COMMAND}
        --test-dir ${CMAKE_BINARY_DIR}
        -R "^kcp_benchmark$"
        --output-on-failure
    DEPENDS
        kcp
        kcp_benchmark_runner
    USES_TERMINAL
)

add_custom_target(benchmark_full
    COMMAND ${CMAKE_SOURCE_DIR}/test/run_isolated.sh
        $<TARGET_FILE:kcp_benchmark_runner>
        $<TARGET_FILE:kcp>
        "${CP_BENCHMARK_CXX}"
        "${CP_BENCHMARK_RUSTC}"
        ${CMAKE_CURRENT_SOURCE_DIR}/cases
    DEPENDS
        kcp
        kcp_benchmark_runner
    USES_TERMINAL
)

add_custom_target(benchmark_full_isolated
    COMMAND ${CMAKE_SOURCE_DIR}/test/run_isolated.sh
        $<TARGET_FILE:kcp_benchmark_runner>
        $<TARGET_FILE:kcp>
        "${CP_BENCHMARK_CXX}"
        "${CP_BENCHMARK_RUSTC}"
        ${CMAKE_CURRENT_SOURCE_DIR}/cases
    DEPENDS
        kcp
        kcp_benchmark_runner
    USES_TERMINAL
)
