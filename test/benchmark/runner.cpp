#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

import std;

namespace {

struct benchmark_case
{
    std::string name{};
    std::string source_name{};
    std::string input{};
    int expected_exit{};
    std::size_t iterations{};
    bool stable_sort{};
};

struct language
{
    std::string name{};
    std::filesystem::path compiler{};
    std::string extension{};
};

struct command_result
{
    int exit_code{};
    long max_rss_kib{};
};

struct run_measurement
{
    std::string case_name{};
    std::string language{};
    double compile_ms{};
    long compile_max_rss_kib{};
    double median_ms{};
    double min_ms{};
    double max_ms{};
    long run_max_rss_kib{};
    std::filesystem::path binary{};
};

auto status_exit_code(int status) -> int
{
    if(WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if(WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 125;
}

auto run_process(std::vector<std::string> const& arguments, std::optional<std::string_view> input = std::nullopt, bool stable_sort = false) -> command_result
{
    auto argv = std::vector<char*>{};
    argv.reserve(arguments.size() + 1uz);
    for(auto const& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);

    auto const pid = ::fork();
    if(pid < 0) {
        return { .exit_code = 125, .max_rss_kib = 0 };
    }

    if(pid == 0) {
        if(auto const null_fd = ::open("/dev/null", O_WRONLY); null_fd >= 0) {
            static_cast<void>(::dup2(null_fd, STDOUT_FILENO));
            static_cast<void>(::dup2(null_fd, STDERR_FILENO));
            if(null_fd > STDERR_FILENO) {
                static_cast<void>(::close(null_fd));
            }
        }

        if(input) {
            auto input_text = std::string{ *input };
            static_cast<void>(::setenv("CP_BENCH_INPUT", input_text.c_str(), 1));
        }
        if(stable_sort) {
            static_cast<void>(::setenv("CP_BENCH_STABLE", "1", 1));
        } else {
            static_cast<void>(::unsetenv("CP_BENCH_STABLE"));
        }

        ::execvp(arguments.front().c_str(), argv.data());
        ::_exit(errno == ENOENT ? 127 : 126);
    }

    auto usage = rusage{};
    auto status = 0;
    while(::wait4(pid, &status, 0, &usage) < 0) {
        if(errno != EINTR) {
            return { .exit_code = 125, .max_rss_kib = usage.ru_maxrss };
        }
    }

    return { .exit_code = status_exit_code(status), .max_rss_kib = usage.ru_maxrss };
}

auto duration_ms(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end) -> double
{
    return std::chrono::duration<double, std::milli>{ end - start }.count();
}

auto require_tool(std::string_view label, std::filesystem::path const& path) -> bool
{
    if(path.empty() or path.string().ends_with("-NOTFOUND")) {
        std::cerr << "benchmark requires " << label << '\n';
        return false;
    }
    return true;
}

auto compile_command(language const& value, std::filesystem::path const& source, std::filesystem::path const& output) -> std::vector<std::string>
{
    if(value.name == "cp") {
        return {
            value.compiler.string(),
            source.string(),
            "--release",
            "-o",
            output.string(),
        };
    }
    if(value.name == "c++") {
        return {
            value.compiler.string(),
            "-std=c++20",
            "-O3",
            "-DNDEBUG",
            source.string(),
            "-o",
            output.string(),
        };
    }
    return {
        value.compiler.string(),
        "-C",
        "opt-level=3",
        "-C",
        "debuginfo=0",
        source.string(),
        "-o",
        output.string(),
    };
}

auto run_command(language const& value, std::filesystem::path const& binary) -> std::vector<std::string>
{
    static_cast<void>(value);
    return { binary.string() };
}

auto artifact_size(std::filesystem::path const& path) -> std::uintmax_t
{
    auto error = std::error_code{};
    if(std::filesystem::is_directory(path, error)) {
        auto total = std::uintmax_t{ 0 };
        for(auto const& entry : std::filesystem::recursive_directory_iterator{ path, error }) {
            if(error) {
                return total;
            }
            if(entry.is_regular_file(error)) {
                total += entry.file_size(error);
            }
        }
        return total;
    }
    auto size = std::filesystem::file_size(path, error);
    if(error) {
        return 0;
    }
    return size;
}

auto measure_case(benchmark_case const& bench, language const& value, std::filesystem::path const& cases_dir, std::filesystem::path const& output_dir) -> std::optional<run_measurement>
{
    auto source_name = bench.source_name.empty() ? bench.name : bench.source_name;
    auto source = cases_dir / source_name / ("case." + value.extension);
    auto binary = output_dir / (bench.name + "-" + value.name);
    auto compile = compile_command(value, source, binary);

    auto compile_start = std::chrono::steady_clock::now();
    auto compile_result = run_process(compile);
    auto compile_end = std::chrono::steady_clock::now();
    if(compile_result.exit_code != 0) {
        std::cerr << "failed to compile " << bench.name << " for " << value.name
                  << " with exit " << compile_result.exit_code
                  << " and max_rss_kib " << compile_result.max_rss_kib << '\n';
        return std::nullopt;
    }

    auto run_peak_rss_kib = 0L;
    for(auto warmup = 0; warmup < 2; ++warmup) {
        auto result = run_process(run_command(value, binary), bench.input, bench.stable_sort);
        run_peak_rss_kib = std::max(run_peak_rss_kib, result.max_rss_kib);
        if(result.exit_code != bench.expected_exit) {
            std::cerr << bench.name << '/' << value.name << " returned " << result.exit_code
                      << ", expected " << bench.expected_exit
                      << ", max_rss_kib " << result.max_rss_kib << '\n';
            return std::nullopt;
        }
    }

    auto samples = std::vector<double>{};
    samples.reserve(bench.iterations);
    for(auto iteration = 0uz; iteration < bench.iterations; ++iteration) {
        auto start = std::chrono::steady_clock::now();
        auto result = run_process(run_command(value, binary), bench.input, bench.stable_sort);
        auto end = std::chrono::steady_clock::now();
        run_peak_rss_kib = std::max(run_peak_rss_kib, result.max_rss_kib);
        if(result.exit_code != bench.expected_exit) {
            std::cerr << bench.name << '/' << value.name << " returned " << result.exit_code
                      << ", expected " << bench.expected_exit
                      << ", max_rss_kib " << result.max_rss_kib << '\n';
            return std::nullopt;
        }
        samples.push_back(duration_ms(start, end));
    }

    std::ranges::sort(samples);
    return run_measurement {
        .case_name = bench.name,
        .language = value.name,
        .compile_ms = duration_ms(compile_start, compile_end),
        .compile_max_rss_kib = compile_result.max_rss_kib,
        .median_ms = samples[samples.size() / 2uz],
        .min_ms = samples.front(),
        .max_ms = samples.back(),
        .run_max_rss_kib = run_peak_rss_kib,
        .binary = binary,
    };
}

auto print_jsonl(run_measurement const& value, benchmark_case const& bench) -> void
{
    std::cout
        << "{\"case\":\"" << value.case_name
        << "\",\"language\":\"" << value.language
        << "\",\"input\":" << bench.input
        << ",\"iterations\":" << bench.iterations
        << ",\"compile_ms\":" << std::fixed << std::setprecision(3) << value.compile_ms
        << ",\"compile_max_rss_kib\":" << value.compile_max_rss_kib
        << ",\"run_median_ms\":" << value.median_ms
        << ",\"run_min_ms\":" << value.min_ms
        << ",\"run_max_ms\":" << value.max_ms
        << ",\"run_max_rss_kib\":" << value.run_max_rss_kib
        << ",\"binary_bytes\":" << artifact_size(value.binary)
        << "}\n";
}

auto print_summary(std::vector<run_measurement> const& measurements) -> void
{
    std::cout << "\ncase                    language   compile_ms   median_ms   run_rss_mb   vs_cpp\n";
    std::cout << "-----------------------------------------------------------------------------\n";

    auto cpp_medians = std::map<std::string, double>{};
    for(auto const& value : measurements) {
        if(value.language == "c++") {
            cpp_medians.emplace(value.case_name, value.median_ms);
        }
    }

    for(auto const& value : measurements) {
        auto ratio = 0.0;
        if(auto iter = cpp_medians.find(value.case_name); iter != cpp_medians.end() and iter->second > 0.0) {
            ratio = value.median_ms / iter->second;
        }
        std::cout
            << std::left << std::setw(24) << value.case_name
            << std::setw(11) << value.language
            << std::right << std::setw(11) << std::fixed << std::setprecision(3) << value.compile_ms
            << std::setw(12) << value.median_ms
            << std::setw(13) << value.run_max_rss_kib / 1024.0
            << std::setw(9) << ratio
            << '\n';
    }
}

} // namespace

auto main(int argc, char** argv) -> int
{
    if(argc < 5) {
        std::cerr << "usage: cp_benchmark_runner <cp> <c++ compiler> <rustc> <cases-dir> [case...]\n";
        return 2;
    }

    auto languages = std::vector<language> {
        { .name = "c++", .compiler = argv[2], .extension = "cpp" },
        { .name = "cp", .compiler = argv[1], .extension = "cp" },
        { .name = "rust", .compiler = argv[3], .extension = "rs" },
    };
    if(not require_tool("cp compiler", languages[1uz].compiler)
        or not require_tool("C++ compiler", languages[0uz].compiler)
        or not require_tool("rustc", languages[2uz].compiler)) {
        return 2;
    }

    auto cases = std::vector<benchmark_case> {
        { .name = "int_loop", .input = "100000000", .expected_exit = 18, .iterations = 10 },
        { .name = "branch_loop", .input = "100000003", .expected_exit = 6, .iterations = 10 },
        { .name = "pointer_walk", .input = "20000000", .expected_exit = 1, .iterations = 10 },
        { .name = "call_dense", .input = "50000000", .expected_exit = 176, .iterations = 10 },
        { .name = "generic_mix", .input = "25000000", .expected_exit = 160, .iterations = 10 },
        { .name = "allocation_churn", .input = "100000", .expected_exit = 243, .iterations = 10 },
        { .name = "std_vector_string", .input = "500000", .expected_exit = 45, .iterations = 10 },
        { .name = "std_map_set", .input = "50000", .expected_exit = 55, .iterations = 10 },
        { .name = "std_sort_int_small", .source_name = "std_sort", .input = "1024", .expected_exit = 19, .iterations = 10 },
        { .name = "std_sort_int_medium", .source_name = "std_sort", .input = "65536", .expected_exit = 230, .iterations = 7 },
        { .name = "std_sort_int_large", .source_name = "std_sort", .input = "1000000", .expected_exit = 194, .iterations = 5 },
        { .name = "std_stable_sort_int_small", .source_name = "std_sort", .input = "1024", .expected_exit = 19, .iterations = 10, .stable_sort = true },
        { .name = "std_stable_sort_int_medium", .source_name = "std_sort", .input = "65536", .expected_exit = 230, .iterations = 7, .stable_sort = true },
        { .name = "std_stable_sort_int_large", .source_name = "std_sort", .input = "1000000", .expected_exit = 194, .iterations = 5, .stable_sort = true },
        { .name = "std_sort_record_small", .source_name = "std_sort_record", .input = "1024", .expected_exit = 161, .iterations = 10 },
        { .name = "std_sort_record_medium", .source_name = "std_sort_record", .input = "32768", .expected_exit = 45, .iterations = 7 },
        { .name = "std_sort_record_large", .source_name = "std_sort_record", .input = "300000", .expected_exit = 32, .iterations = 5 },
        { .name = "std_stable_sort_record_small", .source_name = "std_sort_record", .input = "1024", .expected_exit = 161, .iterations = 10, .stable_sort = true },
        { .name = "std_stable_sort_record_medium", .source_name = "std_sort_record", .input = "32768", .expected_exit = 45, .iterations = 7, .stable_sort = true },
        { .name = "std_stable_sort_record_large", .source_name = "std_sort_record", .input = "300000", .expected_exit = 32, .iterations = 5, .stable_sort = true },
        { .name = "std_sort_string_view_small", .source_name = "std_sort_string", .input = "512", .expected_exit = 53, .iterations = 10 },
        { .name = "std_sort_string_view_medium", .source_name = "std_sort_string", .input = "8192", .expected_exit = 110, .iterations = 7 },
        { .name = "std_sort_string_view_large", .source_name = "std_sort_string", .input = "50000", .expected_exit = 117, .iterations = 5 },
        { .name = "std_stable_sort_string_view_small", .source_name = "std_sort_string", .input = "512", .expected_exit = 53, .iterations = 10, .stable_sort = true },
        { .name = "std_stable_sort_string_view_medium", .source_name = "std_sort_string", .input = "8192", .expected_exit = 110, .iterations = 7, .stable_sort = true },
        { .name = "std_stable_sort_string_view_large", .source_name = "std_sort_string", .input = "50000", .expected_exit = 117, .iterations = 5, .stable_sort = true },
        { .name = "std_sort_string_small", .source_name = "std_sort_owned_string", .input = "512", .expected_exit = 53, .iterations = 10 },
        { .name = "std_sort_string_medium", .source_name = "std_sort_owned_string", .input = "8192", .expected_exit = 110, .iterations = 7 },
        { .name = "std_sort_string_large", .source_name = "std_sort_owned_string", .input = "50000", .expected_exit = 117, .iterations = 5 },
        { .name = "std_stable_sort_string_small", .source_name = "std_sort_owned_string", .input = "512", .expected_exit = 53, .iterations = 10, .stable_sort = true },
        { .name = "std_stable_sort_string_medium", .source_name = "std_sort_owned_string", .input = "8192", .expected_exit = 110, .iterations = 7, .stable_sort = true },
        { .name = "std_stable_sort_string_large", .source_name = "std_sort_owned_string", .input = "50000", .expected_exit = 117, .iterations = 5, .stable_sort = true },
        { .name = "std_sort_unique_string_small", .source_name = "std_sort_unique_string", .input = "512", .expected_exit = 41, .iterations = 10 },
        { .name = "std_sort_unique_string_medium", .source_name = "std_sort_unique_string", .input = "8192", .expected_exit = 121, .iterations = 7 },
        { .name = "std_sort_unique_string_large", .source_name = "std_sort_unique_string", .input = "50000", .expected_exit = 9, .iterations = 5 },
        { .name = "std_stable_sort_unique_string_small", .source_name = "std_sort_unique_string", .input = "512", .expected_exit = 41, .iterations = 10, .stable_sort = true },
        { .name = "std_stable_sort_unique_string_medium", .source_name = "std_sort_unique_string", .input = "8192", .expected_exit = 121, .iterations = 7, .stable_sort = true },
        { .name = "std_stable_sort_unique_string_large", .source_name = "std_sort_unique_string", .input = "50000", .expected_exit = 9, .iterations = 5, .stable_sort = true },
    };
    auto requested_cases = std::set<std::string>{};
    for(auto index = 5; index < argc; ++index) {
        requested_cases.emplace(argv[index]);
    }

    auto cases_dir = std::filesystem::path{ argv[4] };
    auto output_dir = std::filesystem::temp_directory_path() / std::format("cp-benchmark-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(output_dir);

    auto measurements = std::vector<run_measurement>{};
    for(auto const& bench : cases) {
        if(not requested_cases.empty() and not requested_cases.contains(bench.name)) {
            continue;
        }
        for(auto const& value : languages) {
            auto measurement = measure_case(bench, value, cases_dir, output_dir);
            if(not measurement) {
                std::filesystem::remove_all(output_dir);
                return 1;
            }
            print_jsonl(*measurement, bench);
            measurements.push_back(*measurement);
        }
    }
    if(measurements.empty()) {
        std::cerr << "no benchmark cases selected\n";
        std::filesystem::remove_all(output_dir);
        return 2;
    }

    print_summary(measurements);
    std::filesystem::remove_all(output_dir);
    return 0;
}
