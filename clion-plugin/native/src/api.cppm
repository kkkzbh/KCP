export module cp_lexer_helper;

import std;
import source;
import preprocessor;
import lexer;

export namespace cp_lexer_helper {

struct diagnostic_record
{
    std::string code;
    std::string message;
    std::string severity;
    std::size_t start_offset{};
    std::size_t end_offset{};
    std::size_t line{};
    std::size_t column{};
};

struct token_record
{
    std::string kind;
    std::string lexeme;
    std::size_t start_offset{};
    std::size_t end_offset{};
    bool leading_space{};
    bool start_of_line{};
    bool unterminated{};
    bool recovered{};
};

[[nodiscard]]
auto analyze(std::string_view filename, std::string_view text) -> std::vector<diagnostic_record>;

[[nodiscard]]
auto tokenize(std::string_view filename, std::string_view text) -> std::vector<token_record>;

[[nodiscard]]
auto diagnostics_to_json(std::span<diagnostic_record const> diagnostics) -> std::string;

[[nodiscard]]
auto tokens_to_json(std::span<token_record const> tokens) -> std::string;

auto run_cli(std::span<std::string_view const> args, std::istream& input, std::ostream& output,
             std::ostream& error) -> int;

} // namespace cp_lexer_helper

namespace cp_lexer_helper {
namespace {

auto escape_json(std::string_view value) -> std::string
{
    auto result = std::string{};
    result.reserve(value.size() + 8);

    for(auto const ch : value) {
        switch(ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if(static_cast<unsigned char>(ch) < 0x20U) {
                result += std::format("\\u{:04x}", static_cast<unsigned int>(static_cast<unsigned char>(ch)));
            } else {
                result.push_back(ch);
            }
            break;
        }
    }

    return result;
}

auto bool_json(bool value) -> std::string_view
{
    return value ? "true" : "false";
}

auto append_diagnostic_record(
    std::vector<diagnostic_record>& records,
    source_manager const& sources,
    byte_pos file_start,
    diagnostic const& value) -> void
{
    auto const position = sources.position(value.primary_span.start);
    auto const info = spec(value.kind);
    records.emplace_back (
        std::string{ info.code },
        value.message,
        std::string{ severity_name(info.severity) },
        value.primary_span.start - file_start,
        value.primary_span.end - file_start,
        position.line,
        position.column);
}

auto read_all(std::istream& input) -> std::string
{
    return std::string {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

auto usage(std::ostream& error) -> void
{
    error << "usage: cp-lexer-helper <analyze|tokens> --stdin --filename <name> --format json\n";
}

struct cli_request
{
    std::string command;
    std::string filename;
    bool read_stdin{};
    bool json{};
};

auto parse_cli(std::span<std::string_view const> args, std::ostream& error) -> std::optional<cli_request>
{
    if(args.empty()) {
        usage(error);
        return std::nullopt;
    }

    auto request = cli_request{
        .command = std::string{args.front()},
    };

    for(auto index = std::size_t{1}; index < args.size(); ++index) {
        auto const arg = args[index];
        if(arg == "--stdin") {
            request.read_stdin = true;
            continue;
        }
        if(arg == "--format") {
            if(index + 1 >= args.size()) {
                usage(error);
                return std::nullopt;
            }
            request.json = args[++index] == "json";
            continue;
        }
        if(arg == "--filename") {
            if(index + 1 >= args.size()) {
                usage(error);
                return std::nullopt;
            }
            request.filename = std::string{args[++index]};
            continue;
        }
        if(arg == "--help" or arg == "-h") {
            usage(error);
            return std::nullopt;
        }

        error << std::format("unknown argument: {}\n", arg);
        usage(error);
        return std::nullopt;
    }

    if((request.command != "analyze" and request.command != "tokens")
       or not request.read_stdin
       or request.filename.empty()
       or not request.json) {
        usage(error);
        return std::nullopt;
    }

    return request;
}

} // namespace

auto analyze(std::string_view filename, std::string_view text) -> std::vector<diagnostic_record>
{
    auto sources = source_manager{};
    auto const file = sources.add_source(std::string{filename}, std::string{text});
    auto const file_start = sources.file_start(file);
    auto preprocessed = preprocess(sources, file);
    auto lexical = lex(preprocessed);

    auto result = std::vector<diagnostic_record>{};
    result.reserve(preprocessed.diagnostics.size() + lexical.diagnostics.size());

    for(auto const& diagnostic : preprocessed.diagnostics) {
        append_diagnostic_record(result, sources, file_start, diagnostic);
    }

    for(auto const& diagnostic : lexical.diagnostics) {
        append_diagnostic_record(result, sources, file_start, diagnostic);
    }

    return result;
}

auto tokenize(std::string_view filename, std::string_view text) -> std::vector<token_record>
{
    auto sources = source_manager{};
    auto const file = sources.add_source(std::string{filename}, std::string{text});
    auto const file_start = sources.file_start(file);
    auto preprocessed = preprocess(sources, file);
    auto lexical = lex(preprocessed);

    auto result = std::vector<token_record>{};
    for(auto const& token : lexical.tokens) {
        result.emplace_back (
            std::string{to_string(token.kind)},
            std::string{sources.slice(token.span)},
            token.span.start - file_start,
            token.span.end - file_start,
            has_flag(token.flags, token_flags::leading_space),
            has_flag(token.flags, token_flags::start_of_line),
            has_flag(token.flags, token_flags::unterminated),
            has_flag(token.flags, token_flags::recovered));
    }

    return result;
}

auto diagnostics_to_json(std::span<diagnostic_record const> diagnostics) -> std::string
{
    auto json = std::string{"["};
    for(auto index = std::size_t{0}; index < diagnostics.size(); ++index) {
        auto const& diagnostic = diagnostics[index];
        if(index != 0) {
            json += ',';
        }
        json += std::format (
            "{{\"code\":\"{}\",\"message\":\"{}\",\"severity\":\"{}\",\"startOffset\":{},\"endOffset\":{},"
            "\"line\":{},\"column\":{}}}",
            escape_json(diagnostic.code),
            escape_json(diagnostic.message),
            escape_json(diagnostic.severity),
            diagnostic.start_offset,
            diagnostic.end_offset,
            diagnostic.line,
            diagnostic.column);
    }
    json += ']';
    return json;
}

auto tokens_to_json(std::span<token_record const> tokens) -> std::string
{
    auto json = std::string{"["};
    for(auto index = std::size_t{0}; index < tokens.size(); ++index) {
        auto const& token = tokens[index];
        if(index != 0) {
            json += ',';
        }
        json += std::format (
            "{{\"kind\":\"{}\",\"lexeme\":\"{}\",\"startOffset\":{},\"endOffset\":{},"
            "\"leadingSpace\":{},\"startOfLine\":{},\"unterminated\":{},\"recovered\":{}}}",
            escape_json(token.kind),
            escape_json(token.lexeme),
            token.start_offset,
            token.end_offset,
            bool_json(token.leading_space),
            bool_json(token.start_of_line),
            bool_json(token.unterminated),
            bool_json(token.recovered));
    }
    json += ']';
    return json;
}

auto run_cli(std::span<std::string_view const> args, std::istream& input, std::ostream& output,
             std::ostream& error) -> int
{
    auto const request = parse_cli(args, error);
    if(not request) {
        return 2;
    }

    auto const text = read_all(input);
    if(request->command == "analyze") {
        output << diagnostics_to_json(analyze(request->filename, text));
        return 0;
    }

    output << tokens_to_json(tokenize(request->filename, text));
    return 0;
}

} // namespace cp_lexer_helper
