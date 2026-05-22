import std;

namespace {

struct benchmark_case
{
    std::string name{};
    std::string input{};
    int expected_exit{};
    std::size_t iterations{};
};

struct language
{
    std::string name{};
    std::filesystem::path compiler{};
    std::filesystem::path runtime{};
    std::string extension{};
};

struct run_measurement
{
    std::string case_name{};
    std::string language{};
    double compile_ms{};
    double median_ms{};
    double min_ms{};
    double max_ms{};
    std::filesystem::path binary{};
};

auto shell_quote(std::string_view value) -> std::string
{
    auto output = std::string{ "'" };
    for(auto character : value) {
        if(character == '\'') {
            output += "'\\''";
        } else {
            output += character;
        }
    }
    output += '\'';
    return output;
}

auto shell_join(std::vector<std::string> const& arguments) -> std::string
{
    return (
        arguments
        | std::views::transform([](std::string const& value) { return shell_quote(value); })
        | std::views::join_with(' ')
        | std::ranges::to<std::string>()
    );
}

auto exit_code(int status) -> int
{
    if(status < 0) {
        return status;
    }
    return status / 256;
}

auto run_status(std::vector<std::string> const& arguments, std::optional<std::string_view> input = std::nullopt) -> int
{
    auto command = std::string{};
    if(input) {
        command += "CP_BENCH_INPUT=";
        command += shell_quote(*input);
        command += ' ';
    }
    command += shell_join(arguments);
    command += " >/dev/null 2>&1";
    return std::system(command.c_str());
}

auto java_runtime_command(std::filesystem::path const& runtime, std::filesystem::path const& classes) -> std::vector<std::string>
{
    return {
        runtime.string(),
        "-Xms256m",
        "-Xmx256m",
        "-XX:+UseSerialGC",
        "-cp",
        classes.string(),
        "Main",
    };
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
    if(value.name == "java") {
        std::filesystem::create_directories(output);
        return {
            value.compiler.string(),
            "--release",
            "21",
            "-g:none",
            "-encoding",
            "UTF-8",
            "-d",
            output.string(),
            source.string(),
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
    if(value.name == "java") {
        return java_runtime_command(value.runtime, binary);
    }
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
    auto source = cases_dir / bench.name / ("case." + value.extension);
    auto binary = output_dir / (bench.name + "-" + value.name);
    auto compile = compile_command(value, source, binary);

    auto compile_start = std::chrono::steady_clock::now();
    auto compile_status = run_status(compile);
    auto compile_end = std::chrono::steady_clock::now();
    if(compile_status != 0) {
        std::cerr << "failed to compile " << bench.name << " for " << value.name << '\n';
        return std::nullopt;
    }

    for(auto warmup = 0; warmup < 2; ++warmup) {
        auto status = exit_code(run_status(run_command(value, binary), bench.input));
        if(status != bench.expected_exit) {
            std::cerr << bench.name << '/' << value.name << " returned " << status << ", expected " << bench.expected_exit << '\n';
            return std::nullopt;
        }
    }

    auto samples = std::vector<double>{};
    samples.reserve(bench.iterations);
    for(auto iteration = 0uz; iteration < bench.iterations; ++iteration) {
        auto start = std::chrono::steady_clock::now();
        auto status = exit_code(run_status(run_command(value, binary), bench.input));
        auto end = std::chrono::steady_clock::now();
        if(status != bench.expected_exit) {
            std::cerr << bench.name << '/' << value.name << " returned " << status << ", expected " << bench.expected_exit << '\n';
            return std::nullopt;
        }
        samples.push_back(duration_ms(start, end));
    }

    std::ranges::sort(samples);
    return run_measurement {
        .case_name = bench.name,
        .language = value.name,
        .compile_ms = duration_ms(compile_start, compile_end),
        .median_ms = samples[samples.size() / 2uz],
        .min_ms = samples.front(),
        .max_ms = samples.back(),
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
        << ",\"run_median_ms\":" << value.median_ms
        << ",\"run_min_ms\":" << value.min_ms
        << ",\"run_max_ms\":" << value.max_ms
        << ",\"binary_bytes\":" << artifact_size(value.binary)
        << "}\n";
}

auto print_summary(std::vector<run_measurement> const& measurements) -> void
{
    std::cout << "\ncase                 language   compile_ms   median_ms   vs_cpp\n";
    std::cout << "---------------------------------------------------------------\n";

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
            << std::left << std::setw(20) << value.case_name
            << std::setw(11) << value.language
            << std::right << std::setw(11) << std::fixed << std::setprecision(3) << value.compile_ms
            << std::setw(12) << value.median_ms
            << std::setw(9) << ratio
            << '\n';
    }
}

} // namespace

auto main(int argc, char** argv) -> int
{
    if(argc != 7) {
        std::cerr << "usage: cp_benchmark_runner <cp> <c++ compiler> <rustc> <javac> <java> <cases-dir>\n";
        return 2;
    }

    auto languages = std::vector<language> {
        { .name = "c++", .compiler = argv[2], .runtime = {}, .extension = "cpp" },
        { .name = "cp", .compiler = argv[1], .runtime = {}, .extension = "cp" },
        { .name = "rust", .compiler = argv[3], .runtime = {}, .extension = "rs" },
        { .name = "java", .compiler = argv[4], .runtime = argv[5], .extension = "java" },
    };
    if(not require_tool("cp compiler", languages[1uz].compiler)
        or not require_tool("C++ compiler", languages[0uz].compiler)
        or not require_tool("rustc", languages[2uz].compiler)
        or not require_tool("javac", languages[3uz].compiler)
        or not require_tool("java", languages[3uz].runtime)) {
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
    };
    auto cases_dir = std::filesystem::path{ argv[6] };
    auto output_dir = std::filesystem::temp_directory_path() / std::format("cp-benchmark-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(output_dir);

    auto measurements = std::vector<run_measurement>{};
    for(auto const& bench : cases) {
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

    print_summary(measurements);
    std::filesystem::remove_all(output_dir);
    return 0;
}
