find_program(CP_BENCHMARK_CXX NAMES clang++ g++)
find_program(CP_BENCHMARK_RUSTC NAMES rustc)
find_program(CP_BENCHMARK_JAVAC NAMES javac)
find_program(CP_BENCHMARK_JAVA NAMES java)

add_executable(cp_benchmark_runner)

target_sources(cp_benchmark_runner
    PRIVATE
        runner.cpp
)

target_link_libraries(cp_benchmark_runner
    PRIVATE
        stdc++exp
)

set_target_properties(cp_benchmark_runner PROPERTIES
    CXX_STANDARD 26
    CXX_EXTENSIONS OFF
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON
)

add_dependencies(cp_benchmark_runner cp)

add_test(
    NAME cp_benchmark
    COMMAND cp_benchmark_runner
        $<TARGET_FILE:cp>
        "${CP_BENCHMARK_CXX}"
        "${CP_BENCHMARK_RUSTC}"
        "${CP_BENCHMARK_JAVAC}"
        "${CP_BENCHMARK_JAVA}"
        ${CMAKE_CURRENT_SOURCE_DIR}/cases
)

set_tests_properties(cp_benchmark PROPERTIES
    LABELS benchmark
)

add_custom_target(benchmark
    COMMAND ${CMAKE_CTEST_COMMAND}
        --test-dir ${CMAKE_BINARY_DIR}
        -L benchmark
        --verbose
    DEPENDS
        cp
        cp_benchmark_runner
    USES_TERMINAL
)
