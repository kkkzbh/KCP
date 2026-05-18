module;

#include <nlohmann/json.hpp>

export module cp_lexer_helper;

import std;
import source;
import preprocessor;
import lexer;
import parser;
import parser.ast;
import semantic;

export namespace cp_lexer_helper {

struct source_file_record
{
    source_file_record() = default;

    source_file_record(std::string path, std::string text) :
        path(std::move(path)),
        text(std::move(text)) {}

    std::string path;
    std::string text;
};

struct inspect_request
{
    std::string active_file;
    std::vector<source_file_record> files;
};

struct diagnostic_record
{
    std::string stage;
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

struct highlight_record
{
    std::string category;
    std::size_t start_offset{};
    std::size_t end_offset{};
};

struct inspect_result
{
    bool accepted{};
    std::vector<diagnostic_record> diagnostics;
    std::vector<highlight_record> highlights;
};

auto analyze(std::string_view filename, std::string_view text) -> std::vector<diagnostic_record>;

auto tokenize(std::string_view filename, std::string_view text) -> std::vector<token_record>;

auto inspect(inspect_request request) -> inspect_result;

auto diagnostics_to_json(std::span<diagnostic_record const> diagnostics) -> std::string;

auto tokens_to_json(std::span<token_record const> tokens) -> std::string;

auto inspect_to_json(inspect_result const& result) -> std::string;

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

auto diagnostic_stage_name(diagnostic_kind kind) -> std::string
{
    return std::string{ stage_name(spec(kind).stage) };
}

auto make_diagnostic_record(
    source_manager const& sources,
    byte_pos file_start,
    diagnostic const& value
) -> diagnostic_record
{
    auto const position = sources.position(value.primary_span.start);
    auto const info = spec(value.kind);
    return diagnostic_record {
        .stage = diagnostic_stage_name(value.kind),
        .code = std::string{ info.code },
        .message = value.message,
        .severity = std::string{ severity_name(info.severity) },
        .start_offset = value.primary_span.start - file_start,
        .end_offset = value.primary_span.end - file_start,
        .line = position.line,
        .column = position.column,
    };
}

auto append_diagnostic_record(
    std::vector<diagnostic_record>& records,
    source_manager const& sources,
    byte_pos file_start,
    diagnostic const& value) -> void
{
    records.emplace_back(make_diagnostic_record(sources, file_start, value));
}

auto append_diagnostics(
    std::vector<diagnostic>& output,
    std::vector<diagnostic> const& input
) -> void
{
    output.insert(output.end(), input.begin(), input.end());
}

auto span_file(source_manager const& sources, source_span span) -> file_id
{
    return sources.locate(span.start).first;
}

auto span_local_start(source_manager const& sources, source_span span) -> std::size_t
{
    return sources.locate(span.start).second;
}

auto span_local_end(source_manager const& sources, source_span span) -> std::size_t
{
    return sources.locate(span.end).second;
}

auto in_file(source_manager const& sources, source_span span, file_id file) -> bool
{
    return span_file(sources, span) == file;
}

struct highlight_collector
{
    highlight_collector(source_manager const& sources, file_id active_file) :
        sources(sources),
        active_file(active_file) {}

    auto add(std::string_view category, source_span span) -> void
    {
        if(span.start == span.end or not in_file(sources, span, active_file)) {
            return;
        }

        auto const item = highlight_record {
            .category = std::string{ category },
            .start_offset = span_local_start(sources, span),
            .end_offset = span_local_end(sources, span),
        };
        auto const key = std::tuple{ item.category, item.start_offset, item.end_offset };
        if(seen.emplace(key).second) {
            highlights.emplace_back(item);
        }
    }

    source_manager const& sources;
    file_id active_file{};
    std::set<std::tuple<std::string, std::size_t, std::size_t>> seen{};
    std::vector<highlight_record> highlights{};
};

auto is_operator_token(token_kind kind) -> bool
{
    using enum token_kind;
    switch(kind) {
        case plus:
        case plus_equal:
        case minus:
        case minus_equal:
        case star:
        case star_equal:
        case slash:
        case slash_equal:
        case percent:
        case percent_equal:
        case equal:
        case equal_equal:
        case bang_equal:
        case less:
        case less_equal:
        case greater:
        case greater_equal:
        case amp:
        case amp_equal:
        case pipe:
        case pipe_equal:
        case caret:
        case caret_equal:
        case tilde:
        case less_less:
        case less_less_equal:
        case greater_greater:
        case greater_greater_equal:
        case plus_plus:
        case minus_minus:
        case question:
        case kw_as:
        case kw_and:
        case kw_or:
        case kw_not:
            return true;
        default:
            return false;
    }
}

auto is_literal_token(token_kind kind) -> bool
{
    using enum token_kind;
    return (
        kind == integer_literal
        or kind == float_literal
        or kind == char_literal
        or kind == string_literal
        or kind == kw_true
        or kind == kw_false
    );
}

auto add_token_highlights(
    highlight_collector& collector,
    std::span<token const> tokens
) -> void
{
    for(auto const& token : tokens) {
        if(is_literal_token(token.kind)) {
            collector.add("literal", token.span);
            continue;
        }
        if(is_operator_token(token.kind)) {
            collector.add("operator", token.span);
        }
    }
}

auto collect_type_highlights(highlight_collector& collector, ast_arena const& ast, type_id id) -> void
{
    auto const& type = ast.node(id);
    collector.add("type", type.name);
    for(auto const& argument : type.arguments) {
        std::visit(overloaded {
            [&](type_argument_type_syntax const& node) {
                collect_type_highlights(collector, ast, node.type);
            },
            [&](type_argument_literal_syntax const& node) {
                collector.add("literal", node.literal);
            },
        }, argument);
    }
    for(auto associated : type.associated_names) {
        collector.add("type", associated);
    }
}

auto collect_statement_highlights(
    highlight_collector& collector,
    ast_arena const& ast,
    semantic_result const* checked,
    std::size_t unit_index,
    stmt_id id
) -> void;

auto collect_expression_highlights(
    highlight_collector& collector,
    ast_arena const& ast,
    semantic_result const* checked,
    std::size_t unit_index,
    expr_id id,
    bool call_callee = false
) -> void
{
    auto const& expression = ast.node(id);
    std::visit(overloaded {
        [&](name_expr_syntax const& node) {
            if(call_callee) {
                collector.add("function.call", node.name);
                return;
            }

            if(checked != nullptr) {
                auto const symbol = checked->resolved_name(unit_index, id);
                if(symbol.valid() and symbol.value < checked->symbols.size()) {
                    switch(checked->symbols[symbol.value].kind) {
                        case symbol_kind::function:
                            collector.add("function.reference", node.name);
                            return;
                        case symbol_kind::type:
                            collector.add("type", node.name);
                            return;
                        case symbol_kind::concept_:
                            collector.add("type", node.name);
                            return;
                        case symbol_kind::parameter:
                            collector.add("parameter.reference", node.name);
                            return;
                        case symbol_kind::local:
                            collector.add("local.reference", node.name);
                            return;
                    }
                }
            }

            collector.add("identifier.reference", node.name);
        },
        [&](literal_expr_syntax const& node) {
            collector.add("literal", node.full_span);
        },
        [&](unary_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.operand);
        },
        [&](binary_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.left);
            collect_expression_highlights(collector, ast, checked, unit_index, node.right);
        },
        [&](assignment_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.left);
            collect_expression_highlights(collector, ast, checked, unit_index, node.right);
        },
        [&](call_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.callee, true);
            for(auto argument : node.arguments) {
                collect_expression_highlights(collector, ast, checked, unit_index, argument);
            }
        },
        [&](member_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.object);
            collector.add(call_callee ? "function.call" : "field.reference", node.name);
        },
        [&](index_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.object);
            collect_expression_highlights(collector, ast, checked, unit_index, node.index);
        },
        [&](associated_name_expr_syntax const& node) {
            collect_type_highlights(collector, ast, node.type);
            collector.add(call_callee ? "function.call" : "function.reference", node.name);
        },
        [&](cast_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.operand);
            collect_type_highlights(collector, ast, node.type);
        },
        [&](array_literal_expr_syntax const& node) {
            for(auto element : node.elements) {
                collect_expression_highlights(collector, ast, checked, unit_index, element);
            }
        },
        [&](tuple_literal_expr_syntax const& node) {
            for(auto element : node.elements) {
                collect_expression_highlights(collector, ast, checked, unit_index, element);
            }
        },
        [&](grouped_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.expression);
        },
        [&](struct_init_expr_syntax const& node) {
            collect_type_highlights(collector, ast, node.type);
            for(auto const& initializer : node.initializers) {
                if(auto const* named = std::get_if<named_field_initializer_syntax>(&initializer)) {
                    collector.add("field.reference", named->name);
                    collect_expression_highlights(collector, ast, checked, unit_index, named->value);
                } else {
                    collect_expression_highlights(
                        collector,
                        ast,
                        checked,
                        unit_index,
                        std::get<positional_initializer_syntax>(initializer).value
                    );
                }
            }
        },
        [&](block_expr_syntax const& node) {
            for(auto statement : node.statements) {
                collect_statement_highlights(collector, ast, checked, unit_index, statement);
            }
            if(node.tail) {
                collect_expression_highlights(collector, ast, checked, unit_index, *node.tail);
            }
        },
        [&](match_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.value);
            for(auto const& arm : node.arms) {
                std::visit(overloaded {
                    [&](match_case_pattern_syntax const& pattern) {
                        collector.add("variant.case", pattern.name);
                        for(auto binding : pattern.bindings) {
                            collector.add("local.declaration", binding);
                        }
                    },
                    [&](match_wildcard_pattern_syntax const& pattern) {
                        collector.add("local.declaration", pattern.full_span);
                    },
                }, arm.pattern);
                collect_expression_highlights(collector, ast, checked, unit_index, arm.value);
            }
        },
        [&](lambda_expr_syntax const& node) {
            collector.add("function.reference", node.full_span);
        },
    }, expression);
}

auto collect_generic_parameter_highlights(
    highlight_collector& collector,
    std::span<generic_parameter_syntax const> parameters
) -> void
{
    for(auto const& parameter : parameters) {
        collector.add("type.parameter", parameter.name);
        for(auto concept_name : parameter.concept_bounds) {
            collector.add("type", concept_name);
        }
    }
}

auto collect_function_highlights(
    highlight_collector& collector,
    ast_arena const& ast,
    semantic_result const* checked,
    std::size_t unit_index,
    function_syntax const& function
) -> void
{
    collector.add("function.declaration", function.name);
    collect_generic_parameter_highlights(collector, function.generic_parameters);
    for(auto const& parameter : function.parameters) {
        collector.add("parameter.declaration", parameter.name);
        if(parameter.type) {
            collect_type_highlights(collector, ast, *parameter.type);
        }
    }
    if(function.return_type) {
        collect_type_highlights(collector, ast, *function.return_type);
    }
    if(not function.defaulted) {
        collect_statement_highlights(collector, ast, checked, unit_index, function.body);
    }
}

auto collect_statement_highlights(
    highlight_collector& collector,
    ast_arena const& ast,
    semantic_result const* checked,
    std::size_t unit_index,
    stmt_id id
) -> void
{
    auto const& statement = ast.node(id);
    std::visit(overloaded {
        [&](block_statement_syntax const& node) {
            for(auto child : node.statements) {
                collect_statement_highlights(collector, ast, checked, unit_index, child);
            }
        },
        [&](declaration_statement_syntax const& node) {
            collector.add("local.declaration", node.name);
            if(node.declared_type) {
                collect_type_highlights(collector, ast, *node.declared_type);
            }
            collect_expression_highlights(collector, ast, checked, unit_index, node.initializer);
        },
        [&](type_alias_statement_syntax const& node) {
            collector.add("type.declaration", node.name);
            collect_type_highlights(collector, ast, node.type);
        },
        [&](if_statement_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.condition);
            collect_statement_highlights(collector, ast, checked, unit_index, node.then_branch);
            if(node.else_branch) {
                collect_statement_highlights(collector, ast, checked, unit_index, *node.else_branch);
            }
        },
        [&](while_statement_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.condition);
            collect_statement_highlights(collector, ast, checked, unit_index, node.body);
        },
        [&](do_while_statement_syntax const& node) {
            collect_statement_highlights(collector, ast, checked, unit_index, node.body);
            collect_expression_highlights(collector, ast, checked, unit_index, node.condition);
        },
        [&](for_statement_syntax const& node) {
            collector.add("local.declaration", node.name);
            if(node.label) {
                collector.add("loop.label", *node.label);
            }
            collect_expression_highlights(collector, ast, checked, unit_index, node.range);
            collect_statement_highlights(collector, ast, checked, unit_index, node.body);
        },
        [&](break_statement_syntax const& node) {
            if(node.label) {
                collector.add("loop.label.reference", *node.label);
            }
        },
        [&](continue_statement_syntax const& node) {
            if(node.label) {
                collector.add("loop.label.reference", *node.label);
            }
        },
        [&](return_statement_syntax const& node) {
            if(node.value) {
                collect_expression_highlights(collector, ast, checked, unit_index, *node.value);
            }
        },
        [&](expression_statement_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.expression);
        },
    }, statement);
}

auto collect_module_name_highlights(
    highlight_collector& collector,
    module_name_syntax const& name,
    std::string_view category
) -> void
{
    for(auto component : name.components) {
        collector.add(category, component);
    }
}

auto collect_ast_highlights(
    highlight_collector& collector,
    parse_result const& parsed,
    semantic_result const* checked,
    std::size_t unit_index
) -> void
{
    if(not parsed.root) {
        return;
    }

    auto const& root = *parsed.root;
    if(root.module_header) {
        collect_module_name_highlights(collector, root.module_header->name, "module.name");
    }
    for(auto const& import : root.imports) {
        collect_module_name_highlights(collector, import.name, "import.name");
    }

    for(auto id : root.structs) {
        auto const& value = parsed.ast.node(id);
        collector.add("type", value.name);
        collect_generic_parameter_highlights(collector, value.generic_parameters);
        for(auto const& field : value.fields) {
            collector.add("field.declaration", field.name);
            collect_type_highlights(collector, parsed.ast, field.type);
        }
    }
    for(auto id : root.variants) {
        auto const& value = parsed.ast.node(id);
        collector.add("type", value.name);
        collect_generic_parameter_highlights(collector, value.generic_parameters);
        for(auto const& variant_case : value.cases) {
            collector.add("variant.case", variant_case.name);
            for(auto payload : variant_case.payloads) {
                collect_type_highlights(collector, parsed.ast, payload);
            }
        }
    }
    for(auto id : root.concepts) {
        auto const& value = parsed.ast.node(id);
        collector.add("type", value.name);
        for(auto const& item : value.items) {
            std::visit(overloaded {
                [&](concept_requires_syntax const& requirement) {
                    for(auto const& constraint : requirement.constraints) {
                        std::visit(overloaded {
                            [&](concept_parent_constraint_syntax const& parent) {
                                collector.add("type", parent.name);
                            },
                            [&](concept_type_bound_constraint_syntax const& bound) {
                                collect_type_highlights(collector, parsed.ast, bound.type);
                                for(auto concept_name : bound.concept_bounds) {
                                    collector.add("type", concept_name);
                                }
                            },
                            [&](concept_type_equality_constraint_syntax const& equality) {
                                collect_type_highlights(collector, parsed.ast, equality.left);
                                collect_type_highlights(collector, parsed.ast, equality.right);
                            },
                        }, constraint);
                    }
                },
                [&](type_alias_syntax const& alias) {
                    collector.add("type.declaration", alias.name);
                    if(alias.value) {
                        collect_type_highlights(collector, parsed.ast, *alias.value);
                    }
                },
                [&](concept_function_requirement_syntax const& function) {
                    collector.add("function.declaration", function.name);
                    for(auto const& parameter : function.parameters) {
                        collector.add("parameter.declaration", parameter.name);
                        if(parameter.type) {
                            collect_type_highlights(collector, parsed.ast, *parameter.type);
                        }
                    }
                    if(function.return_type) {
                        collect_type_highlights(collector, parsed.ast, *function.return_type);
                    }
                },
            }, item);
        }
    }
    for(auto id : root.impls) {
        auto const& value = parsed.ast.node(id);
        collect_type_highlights(collector, parsed.ast, value.type);
        for(auto const& alias : value.type_aliases) {
            collector.add("type.declaration", alias.name);
            if(alias.value) {
                collect_type_highlights(collector, parsed.ast, *alias.value);
            }
        }
        for(auto function_id : value.functions) {
            auto const& function = parsed.ast.node(function_id);
            collect_function_highlights(collector, parsed.ast, checked, unit_index, function);
        }
    }
    for(auto id : root.concept_impls) {
        auto const& value = parsed.ast.node(id);
        collector.add("type", value.concept_name);
        collect_type_highlights(collector, parsed.ast, value.target_type);
        for(auto const& alias : value.type_aliases) {
            collector.add("type.declaration", alias.name);
            if(alias.value) {
                collect_type_highlights(collector, parsed.ast, *alias.value);
            }
        }
        for(auto function_id : value.functions) {
            auto const& function = parsed.ast.node(function_id);
            collect_function_highlights(collector, parsed.ast, checked, unit_index, function);
        }
    }

    for(auto id : root.functions) {
        auto const& function = parsed.ast.node(id);
        collect_function_highlights(collector, parsed.ast, checked, unit_index, function);
    }
}

auto read_all(std::istream& input) -> std::string
{
    return std::string {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

auto parse_inspect_request(std::string_view text, std::ostream& error) -> std::optional<inspect_request>
{
    try {
        auto const payload = nlohmann::ordered_json::parse(text);
        auto request = inspect_request{};
        request.active_file = payload.value("activeFile", std::string{});

        auto const files = payload.find("files");
        if(files == payload.end() or not files->is_array()) {
            error << "inspect request must contain files array\n";
            return std::nullopt;
        }

        for(auto const& file : *files) {
            if(not file.is_object()) {
                error << "inspect file entry must be an object\n";
                return std::nullopt;
            }
            request.files.emplace_back(
                file.value("path", std::string{}),
                file.value("text", std::string{})
            );
        }

        if(request.active_file.empty() or request.files.empty()) {
            error << "inspect request must contain activeFile and at least one file\n";
            return std::nullopt;
        }

        return request;
    } catch(nlohmann::json::exception const& exception) {
        error << "invalid inspect json: " << exception.what() << '\n';
        return std::nullopt;
    }
}

auto usage(std::ostream& error) -> void
{
    error << "usage: cp-lexer-helper <analyze|tokens|inspect> --stdin [--filename <name>] --format json\n";
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

    auto const known_command = (
        request.command == "analyze"
        or request.command == "tokens"
        or request.command == "inspect"
    );
    auto const needs_filename = request.command != "inspect";

    if(not known_command
       or not request.read_stdin
       or (needs_filename and request.filename.empty())
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

auto inspect(inspect_request request) -> inspect_result
{
    auto sources = source_manager{};
    auto file_ids = std::vector<file_id>{};
    file_ids.reserve(request.files.size());

    auto active_file = std::optional<file_id>{};
    for(auto const& file : request.files) {
        auto const id = sources.add_source(file.path, file.text);
        if(file.path == request.active_file) {
            active_file = id;
        }
        file_ids.emplace_back(id);
    }

    if(not active_file) {
        active_file = file_ids.front();
    }

    auto all_diagnostics = std::vector<diagnostic>{};
    auto parsed_units = std::vector<parse_result>{};
    auto active_unit = std::optional<std::size_t>{};
    auto active_tokens = std::vector<token>{};
    auto frontend_ok = true;

    for(auto index = 0uz; index < file_ids.size(); ++index) {
        auto const file = file_ids[index];
        auto preprocessed = preprocess(sources, file);
        if(not preprocessed.diagnostics.empty()) {
            frontend_ok = false;
            append_diagnostics(all_diagnostics, preprocessed.diagnostics);
            continue;
        }

        auto lexical = lex(preprocessed);
        if(file == *active_file) {
            active_tokens = lexical.tokens;
        }
        if(not lexical.diagnostics.empty()) {
            frontend_ok = false;
            append_diagnostics(all_diagnostics, lexical.diagnostics);
            continue;
        }

        auto parsed = parse_translation_unit(std::move(lexical.tokens));
        if(not parsed.accepted) {
            frontend_ok = false;
        }
        append_diagnostics(all_diagnostics, parsed.diagnostics);

        if(file == *active_file) {
            active_unit = parsed_units.size();
        }
        parsed_units.emplace_back(std::move(parsed));
    }

    auto checked = std::optional<semantic_result>{};
    if(frontend_ok and parsed_units.size() == request.files.size()) {
        checked = analyze_semantics(
            sources,
            std::span<parse_result const>{ parsed_units.data(), parsed_units.size() }
        );
        append_diagnostics(all_diagnostics, checked->diagnostics);
    }

    auto const semantic_ok = checked ? checked->accepted() : frontend_ok;
    auto result = inspect_result {
        .accepted = frontend_ok and semantic_ok,
    };

    auto const active_start = sources.file_start(*active_file);
    for(auto const& diagnostic : all_diagnostics) {
        if(in_file(sources, diagnostic.primary_span, *active_file)) {
            result.diagnostics.emplace_back(make_diagnostic_record(sources, active_start, diagnostic));
        }
    }

    auto collector = highlight_collector{ sources, *active_file };
    add_token_highlights(collector, active_tokens);
    if(active_unit) {
        auto const semantic = checked ? &*checked : nullptr;
        collect_ast_highlights(collector, parsed_units[*active_unit], semantic, *active_unit);
    }
    result.highlights = std::move(collector.highlights);
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
            "{{\"stage\":\"{}\",\"code\":\"{}\",\"message\":\"{}\",\"severity\":\"{}\",\"startOffset\":{},\"endOffset\":{},"
            "\"line\":{},\"column\":{}}}",
            escape_json(diagnostic.stage),
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

auto inspect_to_json(inspect_result const& result) -> std::string
{
    auto payload = nlohmann::ordered_json{};
    payload["accepted"] = result.accepted;
    payload["diagnostics"] = nlohmann::ordered_json::array();
    for(auto const& diagnostic : result.diagnostics) {
        auto& item = payload["diagnostics"].emplace_back();
        item["stage"] = diagnostic.stage;
        item["code"] = diagnostic.code;
        item["message"] = diagnostic.message;
        item["severity"] = diagnostic.severity;
        item["startOffset"] = diagnostic.start_offset;
        item["endOffset"] = diagnostic.end_offset;
        item["line"] = diagnostic.line;
        item["column"] = diagnostic.column;
    }

    payload["highlights"] = nlohmann::ordered_json::array();
    for(auto const& highlight : result.highlights) {
        auto& item = payload["highlights"].emplace_back();
        item["category"] = highlight.category;
        item["startOffset"] = highlight.start_offset;
        item["endOffset"] = highlight.end_offset;
    }

    return payload.dump();
}

auto run_cli(std::span<std::string_view const> args, std::istream& input, std::ostream& output,
             std::ostream& error) -> int
{
    auto const request = parse_cli(args, error);
    if(not request) {
        return 2;
    }

    auto const text = read_all(input);
    if(request->command == "inspect") {
        auto parsed = parse_inspect_request(text, error);
        if(not parsed) {
            return 2;
        }
        output << inspect_to_json(inspect(std::move(*parsed)));
        return 0;
    }

    if(request->command == "analyze") {
        output << diagnostics_to_json(analyze(request->filename, text));
        return 0;
    }

    output << tokens_to_json(tokenize(request->filename, text));
    return 0;
}

} // namespace cp_lexer_helper
