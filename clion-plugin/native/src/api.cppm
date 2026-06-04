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
import compiler.import_resolver;

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

struct resolve_request
{
    std::string active_file;
    std::string target_file;
    std::vector<std::string> entry_files;
    std::vector<std::string> import_roots;
    std::vector<std::string> search_roots;
    std::vector<source_file_record> files;
    bool follow_stdlib_imports{ true };
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

struct navigation_record
{
    std::string category;
    std::size_t source_start_offset{};
    std::size_t source_end_offset{};
    std::string target_file;
    std::size_t target_start_offset{};
    std::size_t target_end_offset{};
};

struct capture_record
{
    capture_record() = default;

    capture_record(std::string capture_name, std::string capture_mode, std::string escape_reason, std::size_t reference_start, std::size_t reference_end, std::size_t source_start, std::size_t source_end, bool is_mutated, bool is_escaped) :
        name(std::move(capture_name)),
        mode(std::move(capture_mode)),
        reason(std::move(escape_reason)),
        reference_start_offset(reference_start),
        reference_end_offset(reference_end),
        source_start_offset(source_start),
        source_end_offset(source_end),
        mutated(is_mutated),
        escaped(is_escaped) {}

    std::string name;
    std::string mode;
    std::string reason;
    std::size_t reference_start_offset{};
    std::size_t reference_end_offset{};
    std::size_t source_start_offset{};
    std::size_t source_end_offset{};
    bool mutated{};
    bool escaped{};
};

struct inspect_result
{
    bool accepted{};
    std::vector<diagnostic_record> diagnostics;
    std::vector<highlight_record> highlights;
    std::vector<capture_record> captures;
    std::vector<navigation_record> navigation;
};

auto analyze(std::string_view filename, std::string_view text) -> std::vector<diagnostic_record>;

auto tokenize(std::string_view filename, std::string_view text) -> std::vector<token_record>;

auto inspect(inspect_request request) -> inspect_result;

auto resolve(resolve_request request) -> inspect_request;

auto diagnostics_to_json(std::span<diagnostic_record const> diagnostics) -> std::string;

auto tokens_to_json(std::span<token_record const> tokens) -> std::string;

auto inspect_to_json(inspect_result const& result) -> std::string;

auto inspect_request_to_json(inspect_request const& request) -> std::string;

auto run_cli(std::span<std::string_view const> args, std::istream& input, std::ostream& output, std::ostream& error) -> int;

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

auto make_diagnostic_record(source_manager const& sources, byte_pos file_start, diagnostic const& value) -> diagnostic_record
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

auto append_diagnostic_record(std::vector<diagnostic_record>& records, source_manager const& sources, byte_pos file_start, diagnostic const& value) -> void
{
    records.emplace_back(make_diagnostic_record(sources, file_start, value));
}

auto append_diagnostics(std::vector<diagnostic>& output, std::vector<diagnostic> const& input) -> void
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

auto highlight_priority(std::string_view category) -> int
{
    if(
        category == "operator"
        or category == "literal"
        or category == "number.literal"
        or category == "boolean.literal"
        or category == "null.literal"
        or category == "string.literal"
        or category == "character.literal"
    ) {
        return 0;
    }
    return 1;
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
        auto const range = std::pair{ item.start_offset, item.end_offset };
        if(auto existing = range_categories.find(range); existing != range_categories.end()) {
            if(existing->second == item.category) {
                return;
            }
            if(highlight_priority(existing->second) >= highlight_priority(item.category)) {
                return;
            }
            seen.erase(std::tuple{ existing->second, item.start_offset, item.end_offset });
            std::erase_if(highlights, [&](highlight_record const& highlight) {
                return highlight.start_offset == item.start_offset and highlight.end_offset == item.end_offset;
            });
            existing->second = item.category;
        } else {
            range_categories.emplace(range, item.category);
        }
        auto const key = std::tuple{ item.category, item.start_offset, item.end_offset };
        if(seen.emplace(key).second) {
            highlights.emplace_back(item);
        }
    }

    auto add_capture(semantic_result const& checked, std::size_t unit_index, expr_id id, source_span reference) -> void
    {
        auto access = checked.lambda_capture_of(unit_index, id);
        if(not access.valid()) {
            return;
        }
        auto lambda = checked.lambda_of(unit_index, access.function);
        if(not lambda.valid() or access.field_index >= lambda.captures.size()) {
            return;
        }
        if(reference.start == reference.end or not in_file(sources, reference, active_file)) {
            return;
        }

        auto const& capture = lambda.captures[access.field_index];
        captures.emplace_back(
            capture.name,
            std::string{ semantic_lambda_capture_mode_name(capture.mode) },
            std::string{ semantic_lambda_escape_reason_name(capture.escape_reason) },
            span_local_start(sources, reference),
            span_local_end(sources, reference),
            in_file(sources, capture.span, active_file) ? span_local_start(sources, capture.span) : 0uz,
            in_file(sources, capture.span, active_file) ? span_local_end(sources, capture.span) : 0uz,
            capture.mutated,
            capture.escaped
        );
    }

    source_manager const& sources;
    file_id active_file{};
    std::set<std::tuple<std::string, std::size_t, std::size_t>> seen{};
    std::map<std::pair<std::size_t, std::size_t>, std::string> range_categories{};
    std::vector<highlight_record> highlights{};
    std::vector<capture_record> captures{};
};

struct navigation_collector
{
    navigation_collector(source_manager const& sources, file_id active_file) :
        sources(sources),
        active_file(active_file) {}

    auto add(std::string_view category, source_span source, source_span target) -> void
    {
        if(source.start == source.end or target.start == target.end or not in_file(sources, source, active_file)) {
            return;
        }

        auto const [target_file, target_start] = sources.locate(target.start);
        auto const target_end = sources.locate(target.end).second;
        auto item = navigation_record {
            .category = std::string{ category },
            .source_start_offset = span_local_start(sources, source),
            .source_end_offset = span_local_end(sources, source),
            .target_file = std::string{ sources.name(target_file) },
            .target_start_offset = target_start,
            .target_end_offset = target_end,
        };
        auto const key = std::tuple {
            item.category,
            item.source_start_offset,
            item.source_end_offset,
            item.target_file,
            item.target_start_offset,
            item.target_end_offset,
        };
        if(seen.emplace(key).second) {
            navigation.emplace_back(std::move(item));
        }
    }

    source_manager const& sources;
    file_id active_file{};
    std::set<std::tuple<std::string, std::size_t, std::size_t, std::string, std::size_t, std::size_t>> seen{};
    std::vector<navigation_record> navigation{};
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
        case bang:
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
        case kw_ref:
        case kw_move:
        case kw_forward:
        case kw_new:
        case kw_delete:
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
        or kind == kw_nullptr
    );
}

auto add_token_highlights(highlight_collector& collector, std::span<token const> tokens) -> void
{
    for(auto const& token : tokens) {
        if(token.kind == token_kind::kw_true or token.kind == token_kind::kw_false) {
            collector.add("boolean.literal", token.span);
            continue;
        }
        if(token.kind == token_kind::kw_nullptr) {
            collector.add("null.literal", token.span);
            continue;
        }
        if(token.kind == token_kind::string_literal) {
            collector.add("string.literal", token.span);
            continue;
        }
        if(token.kind == token_kind::char_literal) {
            collector.add("character.literal", token.span);
            continue;
        }
        if(token.kind == token_kind::integer_literal or token.kind == token_kind::float_literal) {
            collector.add("number.literal", token.span);
            continue;
        }
        if(is_operator_token(token.kind)) {
            collector.add("operator", token.span);
        }
    }
}

auto collect_statement_highlights(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, stmt_id id) -> void;

auto collect_expression_highlights(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, expr_id id, bool call_callee = false) -> void;

auto collect_type_highlights(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, type_id id) -> void;

auto collect_generic_parameter_highlights(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, std::span<generic_parameter_syntax const> parameters) -> void;

auto collect_statement_navigation(navigation_collector& collector, ast_arena const& ast, semantic_result const& checked, std::size_t unit_index, stmt_id id) -> void;

auto collect_expression_navigation(navigation_collector& collector, ast_arena const& ast, semantic_result const& checked, std::size_t unit_index, expr_id id, bool call_callee = false) -> void;

auto collect_type_navigation(navigation_collector& collector, ast_arena const& ast, semantic_result const& checked, std::size_t unit_index, type_id id) -> void;

auto collect_generic_parameter_navigation(navigation_collector& collector, ast_arena const& ast, semantic_result const& checked, std::size_t unit_index, std::span<generic_parameter_syntax const> parameters) -> void;

auto source_text(highlight_collector& collector, source_span span) -> std::string_view
{
    return collector.sources.slice(span);
}

auto collect_literal_highlight(highlight_collector& collector, source_span span) -> void
{
    auto const text = source_text(collector, span);
    if(text == "true" or text == "false") {
        collector.add("boolean.literal", span);
    } else if(text == "nullptr") {
        collector.add("null.literal", span);
    } else if(text.starts_with("\"")) {
        collector.add("string.literal", span);
    } else if(text.starts_with("'")) {
        collector.add("character.literal", span);
    } else {
        collector.add("number.literal", span);
    }
}

auto collect_type_argument_highlight(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, type_argument_syntax const& argument) -> void
{
    std::visit(overloaded {
        [&](type_argument_type_syntax const& node) {
            collect_type_highlights(collector, ast, checked, unit_index, node.type);
        },
        [&](type_argument_literal_syntax const& node) {
            collect_literal_highlight(collector, node.literal);
        },
        [&](type_argument_name_syntax const& node) {
            collector.add("type.parameter", node.name);
        },
    }, argument);
}

auto collect_type_argument_highlights(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, std::span<type_argument_syntax const> arguments) -> void
{
    for(auto const& argument : arguments) {
        collect_type_argument_highlight(collector, ast, checked, unit_index, argument);
    }
}

auto collect_concept_id_highlights(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, concept_id_syntax const& concept_id) -> void
{
    collector.add("concept.reference", concept_id.name);
    collect_type_argument_highlights(collector, ast, checked, unit_index, concept_id.arguments);
}

auto collect_type_highlights(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, type_id id) -> void
{
    auto const& type = ast.node(id);
    auto const name = source_text(collector, type.name);
    if(type.is_function_type or type.is_function_pointer) {
        collector.add("function.type", type.name);
        for(auto const& parameter : type.function_parameters) {
            if(parameter.name) {
                collector.add("parameter.declaration", *parameter.name);
            }
            collect_type_highlights(collector, ast, checked, unit_index, parameter.type);
        }
        collect_type_highlights(collector, ast, checked, unit_index, type.function_return);
    } else if(type.is_decltype) {
        collector.add("decltype", type.name);
        collect_expression_highlights(collector, ast, checked, unit_index, type.decltype_expression);
    } else if(name == "this") {
        collector.add("self.type", type.name);
    } else {
        collector.add("type", type.name);
    }
    for(auto const& argument : type.arguments) {
        collect_type_argument_highlight(collector, ast, checked, unit_index, argument);
    }
    for(auto associated : type.associated_names) {
        collector.add("associated.type.reference", associated);
    }
}

auto function_declaration_category(semantic_result const* checked, std::size_t unit_index, function_id id, function_syntax const& function) -> std::string_view
{
    if(checked != nullptr) {
        auto const symbol = checked->function_symbol_of(unit_index, id);
        if(symbol.valid() and symbol.value < checked->symbols.size()) {
            switch(checked->symbols[symbol.value].function_kind) {
                case semantic_function_kind::free_function:
                    return "function.declaration";
                case semantic_function_kind::constructor:
                    return "constructor.declaration";
                case semantic_function_kind::destructor:
                    return "destructor.declaration";
                case semantic_function_kind::member_function:
                    return "member.function.declaration";
                case semantic_function_kind::associated_function:
                    return "associated.function.declaration";
            }
        }
    }
    if(function.kind == function_syntax_kind::constructor) {
        return "constructor.declaration";
    }
    if(function.kind == function_syntax_kind::destructor) {
        return "destructor.declaration";
    }
    return "function.declaration";
}

auto local_category(semantic_result const* checked, symbol_id symbol, std::string_view fallback) -> std::string_view
{
    if(checked != nullptr and symbol.valid() and symbol.value < checked->symbols.size()) {
        auto const& value = checked->symbols[symbol.value];
        if(value.kind == symbol_kind::local and value.is_const) {
            return fallback.ends_with(".reference") ? "constant.reference" : "constant.declaration";
        }
    }
    return fallback;
}

auto lambda_capture_category(semantic_result const& checked, std::size_t unit_index, expr_id id) -> std::string
{
    auto access = checked.lambda_capture_of(unit_index, id);
    if(not access.valid()) {
        return "lambda.capture.reference";
    }
    auto lambda = checked.lambda_of(unit_index, access.function);
    if(not lambda.valid() or access.field_index >= lambda.captures.size()) {
        return "lambda.capture.reference";
    }
    return std::format("lambda.capture.{}", semantic_lambda_capture_mode_name(lambda.captures[access.field_index].mode));
}

auto collect_expression_highlights(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, expr_id id, bool call_callee) -> void
{
    auto const& expression = ast.node(id);
    auto const semantic_symbol = [&]() -> symbol_id {
        if(checked == nullptr) {
            return {};
        }
        auto symbol = checked->resolved_name(unit_index, id);
        if(symbol.valid() and symbol.value < checked->symbols.size()) {
            return symbol;
        }
        return {};
    };
    std::visit(overloaded {
        [&](name_expr_syntax const& node) {
            if(checked != nullptr and checked->field_access_of(unit_index, id).valid()) {
                collector.add("field.reference", node.name);
                return;
            }
            if(checked != nullptr and checked->lambda_capture_of(unit_index, id).valid()) {
                collector.add(lambda_capture_category(*checked, unit_index, id), node.name);
                collector.add_capture(*checked, unit_index, id, node.name);
                return;
            }
            if(auto const symbol = semantic_symbol(); symbol.valid()) {
                switch(checked->symbols[symbol.value].kind) {
                    case symbol_kind::function:
                        collector.add(call_callee ? "function.call" : "function.reference", node.name);
                        return;
                    case symbol_kind::type:
                        collector.add("type", node.name);
                        return;
                    case symbol_kind::concept_:
                        collector.add("concept.reference", node.name);
                        return;
                    case symbol_kind::parameter:
                        collector.add("parameter.reference", node.name);
                        return;
                    case symbol_kind::local:
                        collector.add(local_category(checked, symbol, "local.reference"), node.name);
                        return;
                }
            }

            collector.add("identifier.reference", node.name);
        },
        [&](literal_expr_syntax const& node) {
            collect_literal_highlight(collector, node.full_span);
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
            collect_type_argument_highlights(collector, ast, checked, unit_index, node.type_arguments);
            auto const builtin_call = [&]() -> bool {
                if(checked == nullptr) {
                    return false;
                }
                return checked->builtin_calls.find(semantic_node_key{unit_index, id}) != checked->builtin_calls.end();
            }();
            auto const builtin_escape_call = [&]() -> bool {
                if(auto const* name = std::get_if<name_expr_syntax>(&ast.node(node.callee))) {
                    return source_text(collector, name->name) == "builtin";
                }
                return false;
            }();
            if(builtin_call or builtin_escape_call) {
                if(auto const* name = std::get_if<name_expr_syntax>(&ast.node(node.callee))) {
                    collector.add("builtin.function.call", name->name);
                } else {
                    collect_expression_highlights(collector, ast, checked, unit_index, node.callee, true);
                }
                for(auto argument : node.arguments) {
                    collect_expression_highlights(collector, ast, checked, unit_index, argument);
                }
                return;
            }
            collect_expression_highlights(collector, ast, checked, unit_index, node.callee, true);
            for(auto argument : node.arguments) {
                collect_expression_highlights(collector, ast, checked, unit_index, argument);
            }
        },
        [&](member_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.object);
            if(call_callee) {
                collector.add("member.function.call", node.name);
                return;
            }
            collector.add("field.reference", node.name);
        },
        [&](index_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.object);
            collect_expression_highlights(collector, ast, checked, unit_index, node.index);
        },
        [&](associated_name_expr_syntax const& node) {
            collect_type_highlights(collector, ast, checked, unit_index, node.type);
            if(checked != nullptr and checked->variant_case_of(unit_index, id).valid()) {
                collector.add("variant.case", node.name);
                return;
            }
            if(checked != nullptr and checked->enum_case_of(unit_index, id).valid()) {
                collector.add("enum.case", node.name);
                return;
            }
            if(auto const symbol = semantic_symbol(); symbol.valid()) {
                auto const& value = checked->symbols[symbol.value];
                if(value.function_kind == semantic_function_kind::associated_function) {
                    collector.add(call_callee ? "associated.function.call" : "associated.function.reference", node.name);
                    return;
                }
            }
            collector.add(call_callee ? "associated.function.call" : "associated.function.reference", node.name);
        },
        [&](cast_expr_syntax const& node) {
            collect_expression_highlights(collector, ast, checked, unit_index, node.operand);
            collect_type_highlights(collector, ast, checked, unit_index, node.type);
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
            collect_type_highlights(collector, ast, checked, unit_index, node.type);
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
        [&](new_expr_syntax const& node) {
            collect_type_highlights(collector, ast, checked, unit_index, node.type);
            collect_expression_highlights(collector, ast, checked, unit_index, node.initializer);
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
                            collector.add("pattern.binding", binding);
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
            auto const& function = ast.node(node.function);
            collector.add("lambda.marker", function.name);
            collect_generic_parameter_highlights(collector, ast, checked, unit_index, function.generic_parameters);
            for(auto const& parameter : function.parameters) {
                collector.add("parameter.declaration", parameter.name);
                if(parameter.type) {
                    collect_type_highlights(collector, ast, checked, unit_index, *parameter.type);
                }
                if(parameter.default_value) {
                    collect_expression_highlights(collector, ast, checked, unit_index, *parameter.default_value);
                }
            }
            if(function.return_type) {
                collect_type_highlights(collector, ast, checked, unit_index, *function.return_type);
            }
            collect_statement_highlights(collector, ast, checked, unit_index, function.body);
        },
    }, expression);
}

auto collect_generic_parameter_highlights(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, std::span<generic_parameter_syntax const> parameters) -> void
{
    for(auto const& parameter : parameters) {
        collector.add(parameter.is_pack ? "type.parameter.pack" : "type.parameter", parameter.name);
        for(auto const& concept_bound : parameter.concept_bounds) {
            collect_concept_id_highlights(collector, ast, checked, unit_index, concept_bound);
        }
        if(parameter.default_argument) {
            collect_type_argument_highlight(collector, ast, checked, unit_index, *parameter.default_argument);
        }
    }
}

auto collect_function_highlights(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, function_id id, function_syntax const& function) -> void
{
    collector.add(function_declaration_category(checked, unit_index, id, function), function.name);
    collect_generic_parameter_highlights(collector, ast, checked, unit_index, function.generic_parameters);
    for(auto const& parameter : function.parameters) {
        collector.add("parameter.declaration", parameter.name);
        if(parameter.type) {
            collect_type_highlights(collector, ast, checked, unit_index, *parameter.type);
        }
        if(parameter.default_value) {
            collect_expression_highlights(collector, ast, checked, unit_index, *parameter.default_value);
        }
    }
    if(function.return_type) {
        collect_type_highlights(collector, ast, checked, unit_index, *function.return_type);
    }
    if(function.has_body) {
        collect_statement_highlights(collector, ast, checked, unit_index, function.body);
    }
}

auto collect_statement_highlights(highlight_collector& collector, ast_arena const& ast, semantic_result const* checked, std::size_t unit_index, stmt_id id) -> void
{
    auto const& statement = ast.node(id);
    std::visit(overloaded {
        [&](block_statement_syntax const& node) {
            for(auto child : node.statements) {
                collect_statement_highlights(collector, ast, checked, unit_index, child);
            }
        },
        [&](declaration_statement_syntax const& node) {
            if(node.binding_names.empty()) {
                auto const symbol = checked == nullptr ? symbol_id{} : checked->local_binding_of(unit_index, node.name);
                collector.add(local_category(checked, symbol, "local.declaration"), node.name);
            } else {
                for(auto binding : node.binding_names) {
                    auto const symbol = checked == nullptr ? symbol_id{} : checked->local_binding_of(unit_index, binding);
                    collector.add(local_category(checked, symbol, "local.declaration"), binding);
                }
            }
            if(node.declared_type) {
                collect_type_highlights(collector, ast, checked, unit_index, *node.declared_type);
            }
            collect_expression_highlights(collector, ast, checked, unit_index, node.initializer);
        },
        [&](type_alias_statement_syntax const& node) {
            collector.add("type.alias.declaration", node.name);
            collect_type_highlights(collector, ast, checked, unit_index, node.type);
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
            auto const symbol = checked == nullptr ? symbol_id{} : checked->binding_of(unit_index, id);
            collector.add(local_category(checked, symbol, "local.declaration"), node.name);
            if(node.label) {
                collector.add("loop.label", *node.label);
            }
            collect_expression_highlights(collector, ast, checked, unit_index, node.range);
            collect_statement_highlights(collector, ast, checked, unit_index, node.body);
        },
        [&](template_for_statement_syntax const& node) {
            collector.add(
                node.binding_kind == template_for_binding_kind::type_binding
                    ? "type.parameter"
                    : "local.declaration",
                node.name
            );
            collector.add("parameter.pack.reference", node.pack_name);
            collect_statement_highlights(collector, ast, checked, unit_index, node.body);
        },
        [&](template_if_statement_syntax const& node) {
            for(auto const& condition : node.conditions) {
                if(condition.expression) {
                    collect_expression_highlights(collector, ast, checked, unit_index, *condition.expression);
                }
            }
            for(auto const& branch : node.branches) {
                collect_statement_highlights(collector, ast, checked, unit_index, branch.body);
            }
            if(node.else_branch) {
                collect_statement_highlights(collector, ast, checked, unit_index, *node.else_branch);
            }
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

auto collect_module_name_highlights(highlight_collector& collector, module_name_syntax const& name, std::string_view category) -> void
{
    for(auto component : name.components) {
        collector.add(category, component);
    }
}

auto collect_ast_highlights(highlight_collector& collector, parse_result const& parsed, semantic_result const* checked, std::size_t unit_index) -> void
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
        collect_generic_parameter_highlights(collector, parsed.ast, checked, unit_index, value.generic_parameters);
        for(auto const& field : value.fields) {
            collector.add("field.declaration", field.name);
            collect_type_highlights(collector, parsed.ast, checked, unit_index, field.type);
            if(field.default_value) {
                collect_expression_highlights(collector, parsed.ast, checked, unit_index, *field.default_value);
            }
        }
    }
    for(auto id : root.enums) {
        auto const& value = parsed.ast.node(id);
        collector.add("type", value.name);
        collect_type_highlights(collector, parsed.ast, checked, unit_index, value.underlying_type);
        for(auto const& enum_case : value.cases) {
            collector.add("enum.case", enum_case.name);
            collect_expression_highlights(collector, parsed.ast, checked, unit_index, enum_case.value);
        }
    }
    for(auto id : root.variants) {
        auto const& value = parsed.ast.node(id);
        collector.add("type", value.name);
        collect_generic_parameter_highlights(collector, parsed.ast, checked, unit_index, value.generic_parameters);
        for(auto const& variant_case : value.cases) {
            collector.add("variant.case", variant_case.name);
            for(auto payload : variant_case.payloads) {
                collect_type_highlights(collector, parsed.ast, checked, unit_index, payload);
            }
        }
    }
    for(auto id : root.concepts) {
        auto const& value = parsed.ast.node(id);
        collector.add("concept.declaration", value.name);
        collect_generic_parameter_highlights(collector, parsed.ast, checked, unit_index, value.generic_parameters);
        for(auto const& item : value.items) {
            std::visit(overloaded {
                [&](concept_requires_syntax const& requirement) {
                    for(auto const& constraint : requirement.constraints) {
                        std::visit(overloaded {
                            [&](concept_parent_constraint_syntax const& parent) {
                                collect_concept_id_highlights(collector, parsed.ast, checked, unit_index, parent.parent);
                            },
                            [&](concept_type_bound_constraint_syntax const& bound) {
                                collect_type_highlights(collector, parsed.ast, checked, unit_index, bound.type);
                                for(auto const& concept_bound : bound.concept_bounds) {
                                    collect_concept_id_highlights(collector, parsed.ast, checked, unit_index, concept_bound);
                                }
                            },
                            [&](concept_type_equality_constraint_syntax const& equality) {
                                collect_type_highlights(collector, parsed.ast, checked, unit_index, equality.left);
                                collect_type_highlights(collector, parsed.ast, checked, unit_index, equality.right);
                            },
                        }, constraint);
                    }
                },
                [&](type_alias_syntax const& alias) {
                    collector.add(alias.value ? "associated.type.declaration" : "associated.type.requirement", alias.name);
                    if(alias.value) {
                        collect_type_highlights(collector, parsed.ast, checked, unit_index, *alias.value);
                    }
                },
                [&](concept_function_requirement_syntax const& function) {
                    if(function.default_function) {
                        auto const& default_function = parsed.ast.node(*function.default_function);
                        collect_function_highlights(
                            collector,
                            parsed.ast,
                            checked,
                            unit_index,
                            *function.default_function,
                            default_function
                        );
                        return;
                    }
                    collector.add("concept.function.requirement", function.name);
                    for(auto const& parameter : function.parameters) {
                        collector.add("parameter.declaration", parameter.name);
                        if(parameter.type) {
                            collect_type_highlights(collector, parsed.ast, checked, unit_index, *parameter.type);
                        }
                        if(parameter.default_value) {
                            collect_expression_highlights(collector, parsed.ast, checked, unit_index, *parameter.default_value);
                        }
                    }
                    if(function.return_type) {
                        collect_type_highlights(collector, parsed.ast, checked, unit_index, *function.return_type);
                    }
                },
            }, item);
        }
    }
    for(auto id : root.type_aliases) {
        auto const& value = parsed.ast.node(id);
        collector.add(value.opaque ? "opaque.type.declaration" : "type.alias.declaration", value.name);
        if(value.value) {
            collect_type_highlights(collector, parsed.ast, checked, unit_index, *value.value);
        }
    }
    for(auto id : root.impls) {
        auto const& value = parsed.ast.node(id);
        collect_generic_parameter_highlights(collector, parsed.ast, checked, unit_index, value.generic_parameters);
        collect_type_highlights(collector, parsed.ast, checked, unit_index, value.type);
        for(auto const& alias : value.type_aliases) {
            collector.add("associated.type.declaration", alias.name);
            if(alias.value) {
                collect_type_highlights(collector, parsed.ast, checked, unit_index, *alias.value);
            }
        }
        for(auto function_id : value.functions) {
            auto const& function = parsed.ast.node(function_id);
            collect_function_highlights(collector, parsed.ast, checked, unit_index, function_id, function);
        }
    }
    for(auto id : root.concept_impls) {
        auto const& value = parsed.ast.node(id);
        collect_generic_parameter_highlights(collector, parsed.ast, checked, unit_index, value.generic_parameters);
        collect_concept_id_highlights(collector, parsed.ast, checked, unit_index, value.concept_name);
        collect_type_highlights(collector, parsed.ast, checked, unit_index, value.target_type);
        for(auto const& alias : value.type_aliases) {
            collector.add("associated.type.declaration", alias.name);
            if(alias.value) {
                collect_type_highlights(collector, parsed.ast, checked, unit_index, *alias.value);
            }
        }
        for(auto function_id : value.functions) {
            auto const& function = parsed.ast.node(function_id);
            collect_function_highlights(collector, parsed.ast, checked, unit_index, function_id, function);
        }
    }

    for(auto id : root.functions) {
        auto const& function = parsed.ast.node(id);
        collect_function_highlights(collector, parsed.ast, checked, unit_index, id, function);
    }
}

auto valid_symbol(semantic_result const& checked, symbol_id symbol) -> bool
{
    return symbol.valid() and symbol.value < checked.symbols.size();
}

auto type_declaration_span(semantic_result const& checked, semantic_type_id id) -> std::optional<source_span>
{
    if(not checked.types.contains(id)) {
        return std::nullopt;
    }
    auto const& kind = checked.types.get(id);
    return std::visit(overloaded {
        [&](struct_type const& value) -> std::optional<source_span> {
            if(value.index < checked.structs.size()) {
                return checked.structs[value.index].span;
            }
            return std::nullopt;
        },
        [&](enum_type const& value) -> std::optional<source_span> {
            if(value.index < checked.enums.size()) {
                return checked.enums[value.index].span;
            }
            return std::nullopt;
        },
        [&](opaque_type const& value) -> std::optional<source_span> {
            if(value.index < checked.opaque_aliases.size()) {
                return checked.opaque_aliases[value.index].span;
            }
            return std::nullopt;
        },
        [&](variant_type const& value) -> std::optional<source_span> {
            if(value.index < checked.variants.size()) {
                return checked.variants[value.index].span;
            }
            return std::nullopt;
        },
        [](auto const&) -> std::optional<source_span> {
            return std::nullopt;
        },
    }, kind);
}

auto field_declaration_span(semantic_result const& checked, semantic_field_access access) -> std::optional<source_span>
{
    if(access.struct_index < checked.structs.size()) {
        auto const& fields = checked.structs[access.struct_index].fields;
        if(access.field_index < fields.size()) {
            return fields[access.field_index].span;
        }
    }
    return std::nullopt;
}

auto variant_case_declaration_span(semantic_result const& checked, semantic_variant_case_access access) -> std::optional<source_span>
{
    if(access.variant_index < checked.variants.size()) {
        auto const& cases = checked.variants[access.variant_index].cases;
        if(access.case_index < cases.size()) {
            return cases[access.case_index].span;
        }
    }
    return std::nullopt;
}

auto enum_case_declaration_span(semantic_result const& checked, semantic_enum_case_access access) -> std::optional<source_span>
{
    if(access.enum_index < checked.enums.size()) {
        auto const& cases = checked.enums[access.enum_index].cases;
        if(access.case_index < cases.size()) {
            return cases[access.case_index].span;
        }
    }
    return std::nullopt;
}

auto module_name_text(source_manager const& sources, module_name_syntax const& name) -> std::string
{
    return ast_source_view{sources}.module_name(name);
}

auto module_declaration_spans(source_manager const& sources, std::span<parse_result const> parsed_units) -> std::map<std::string, source_span>
{
    auto declarations = std::map<std::string, source_span>{};
    for(auto const& parsed : parsed_units) {
        if(not parsed.root or not parsed.root->module_header) {
            continue;
        }
        auto const& name = parsed.root->module_header->name;
        declarations.emplace(module_name_text(sources, name), name.full_span);
    }
    return declarations;
}

auto collect_import_navigation(navigation_collector& collector, std::span<parse_result const> parsed_units, std::size_t unit_index) -> void
{
    auto const declarations = module_declaration_spans(collector.sources, parsed_units);
    auto const& parsed = parsed_units[unit_index];
    if(not parsed.root) {
        return;
    }

    for(auto const& import : parsed.root->imports) {
        auto const found = declarations.find(module_name_text(collector.sources, import.name));
        if(found != declarations.end()) {
            collector.add("import.name", import.name.full_span, found->second);
        }
    }
}

auto exact_symbol_span(semantic_result const& checked, std::string_view name, symbol_kind kind) -> std::optional<source_span>
{
    auto found = std::optional<source_span>{};
    for(auto const& symbol : checked.symbols) {
        if(symbol.kind != kind or symbol.name != name) {
            continue;
        }
        if(found) {
            return std::nullopt;
        }
        found = symbol.span;
    }
    return found;
}

auto add_symbol_navigation(navigation_collector& collector, semantic_result const& checked, std::string_view category, source_span source, symbol_id symbol) -> void
{
    if(valid_symbol(checked, symbol)) {
        collector.add(category, source, checked.symbols[symbol.value].span);
    }
}

auto collect_type_arguments_navigation(navigation_collector& collector, ast_arena const& ast, semantic_result const& checked, std::size_t unit_index, std::vector<type_argument_syntax> const& arguments) -> void
{
    for(auto const& argument : arguments) {
        std::visit(overloaded {
            [&](type_argument_type_syntax const& node) {
                collect_type_navigation(collector, ast, checked, unit_index, node.type);
            },
            [](type_argument_literal_syntax const&) {},
            [](type_argument_name_syntax const&) {},
        }, argument);
    }
}

auto collect_concept_id_navigation(navigation_collector& collector, ast_arena const& ast, semantic_result const& checked, std::size_t unit_index, concept_id_syntax const& concept_id) -> void
{
    if(auto target = exact_symbol_span(checked, collector.sources.slice(concept_id.name), symbol_kind::concept_)) {
        collector.add("concept.reference", concept_id.name, *target);
    }
    collect_type_arguments_navigation(collector, ast, checked, unit_index, concept_id.arguments);
}

auto collect_type_navigation(navigation_collector& collector, ast_arena const& ast, semantic_result const& checked, std::size_t unit_index, type_id id) -> void
{
    auto const& type = ast.node(id);
    if(auto target = exact_symbol_span(checked, collector.sources.slice(type.name), symbol_kind::type)) {
        collector.add("type.reference", type.name, *target);
    } else if(auto concept_target = exact_symbol_span(checked, collector.sources.slice(type.name), symbol_kind::concept_)) {
        collector.add("concept.reference", type.name, *concept_target);
    }
    collect_type_arguments_navigation(collector, ast, checked, unit_index, type.arguments);
}

auto collect_expression_navigation(navigation_collector& collector, ast_arena const& ast, semantic_result const& checked, std::size_t unit_index, expr_id id, bool call_callee) -> void
{
    auto const& expression = ast.node(id);
    std::visit(overloaded {
        [&](name_expr_syntax const& node) {
            if(auto access = checked.field_access_of(unit_index, id); access.valid()) {
                if(auto target = field_declaration_span(checked, access)) {
                    collector.add("field.reference", node.name, *target);
                }
                return;
            }
            if(auto symbol = checked.resolved_name(unit_index, id); symbol.valid()) {
                add_symbol_navigation(collector, checked, call_callee ? "function.call" : "name.reference", node.name, symbol);
            }
        },
        [](literal_expr_syntax const&) {},
        [&](unary_expr_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.operand);
        },
        [&](binary_expr_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.left);
            collect_expression_navigation(collector, ast, checked, unit_index, node.right);
        },
        [&](assignment_expr_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.left);
            collect_expression_navigation(collector, ast, checked, unit_index, node.right);
        },
        [&](call_expr_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.callee, true);
            for(auto argument : node.arguments) {
                collect_expression_navigation(collector, ast, checked, unit_index, argument);
            }
        },
        [&](member_expr_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.object);
            if(call_callee) {
                if(auto symbol = checked.resolved_name(unit_index, id); symbol.valid()) {
                    add_symbol_navigation(collector, checked, "member.function.call", node.name, symbol);
                }
                return;
            }
            if(auto access = checked.field_access_of(unit_index, id); access.valid()) {
                if(auto target = field_declaration_span(checked, access)) {
                    collector.add("field.reference", node.name, *target);
                }
            }
        },
        [&](index_expr_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.object);
            collect_expression_navigation(collector, ast, checked, unit_index, node.index);
        },
        [&](associated_name_expr_syntax const& node) {
            collect_type_navigation(collector, ast, checked, unit_index, node.type);
            if(auto access = checked.variant_case_of(unit_index, id); access.valid()) {
                if(auto target = variant_case_declaration_span(checked, access)) {
                    collector.add("variant.case", node.name, *target);
                }
                return;
            }
            if(auto access = checked.enum_case_of(unit_index, id); access.valid()) {
                if(auto target = enum_case_declaration_span(checked, access)) {
                    collector.add("enum.case", node.name, *target);
                }
                return;
            }
            if(auto symbol = checked.resolved_name(unit_index, id); symbol.valid()) {
                add_symbol_navigation(collector, checked, call_callee ? "associated.function.call" : "associated.function.reference", node.name, symbol);
            }
        },
        [&](cast_expr_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.operand);
            collect_type_navigation(collector, ast, checked, unit_index, node.type);
        },
        [&](array_literal_expr_syntax const& node) {
            for(auto element : node.elements) {
                collect_expression_navigation(collector, ast, checked, unit_index, element);
            }
        },
        [&](tuple_literal_expr_syntax const& node) {
            for(auto element : node.elements) {
                collect_expression_navigation(collector, ast, checked, unit_index, element);
            }
        },
        [&](grouped_expr_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.expression);
        },
        [&](struct_init_expr_syntax const& node) {
            if(auto target = type_declaration_span(checked, checked.type_of(unit_index, id))) {
                collector.add("type.reference", ast.node(node.type).name, *target);
                collect_type_arguments_navigation(collector, ast, checked, unit_index, ast.node(node.type).arguments);
            } else {
                collect_type_navigation(collector, ast, checked, unit_index, node.type);
            }

            auto const type = checked.type_of(unit_index, id);
            auto const field_target = [&](source_span name) -> std::optional<source_span> {
                if(not checked.types.contains(type)) {
                    return std::nullopt;
                }
                auto const* structure = std::get_if<struct_type>(&checked.types.get(type));
                if(structure == nullptr or structure->index >= checked.structs.size()) {
                    return std::nullopt;
                }
                auto const& fields = checked.structs[structure->index].fields;
                auto found = std::ranges::find_if(fields, [&](semantic_struct_field const& field) {
                    return field.name == collector.sources.slice(name);
                });
                if(found == fields.end()) {
                    return std::nullopt;
                }
                return found->span;
            };

            for(auto const& initializer : node.initializers) {
                if(auto const* named = std::get_if<named_field_initializer_syntax>(&initializer)) {
                    if(auto target = field_target(named->name)) {
                        collector.add("field.reference", named->name, *target);
                    }
                    collect_expression_navigation(collector, ast, checked, unit_index, named->value);
                } else {
                    collect_expression_navigation(
                        collector,
                        ast,
                        checked,
                        unit_index,
                        std::get<positional_initializer_syntax>(initializer).value
                    );
                }
            }
        },
        [&](new_expr_syntax const& node) {
            collect_type_navigation(collector, ast, checked, unit_index, node.type);
            collect_expression_navigation(collector, ast, checked, unit_index, node.initializer);
        },
        [&](block_expr_syntax const& node) {
            for(auto statement : node.statements) {
                collect_statement_navigation(collector, ast, checked, unit_index, statement);
            }
            if(node.tail) {
                collect_expression_navigation(collector, ast, checked, unit_index, *node.tail);
            }
        },
        [&](match_expr_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.value);
            for(auto const& arm : node.arms) {
                collect_expression_navigation(collector, ast, checked, unit_index, arm.value);
            }
        },
        [&](lambda_expr_syntax const& node) {
            auto const& function = ast.node(node.function);
            collect_generic_parameter_navigation(collector, ast, checked, unit_index, function.generic_parameters);
            for(auto const& parameter : function.parameters) {
                if(parameter.type) {
                    collect_type_navigation(collector, ast, checked, unit_index, *parameter.type);
                }
                if(parameter.default_value) {
                    collect_expression_navigation(collector, ast, checked, unit_index, *parameter.default_value);
                }
            }
            if(function.return_type) {
                collect_type_navigation(collector, ast, checked, unit_index, *function.return_type);
            }
            collect_statement_navigation(collector, ast, checked, unit_index, function.body);
        },
    }, expression);
}

auto collect_generic_parameter_navigation(navigation_collector& collector, ast_arena const& ast, semantic_result const& checked, std::size_t unit_index, std::span<generic_parameter_syntax const> parameters) -> void
{
    for(auto const& parameter : parameters) {
        for(auto const& concept_bound : parameter.concept_bounds) {
            collect_concept_id_navigation(collector, ast, checked, unit_index, concept_bound);
        }
        if(parameter.default_argument) {
            std::visit(overloaded {
                [&](type_argument_type_syntax const& node) {
                    collect_type_navigation(collector, ast, checked, unit_index, node.type);
                },
                [](type_argument_literal_syntax const&) {},
                [](type_argument_name_syntax const&) {},
            }, *parameter.default_argument);
        }
    }
}

auto collect_function_navigation(navigation_collector& collector, ast_arena const& ast, semantic_result const& checked, std::size_t unit_index, function_syntax const& function) -> void
{
    collect_generic_parameter_navigation(collector, ast, checked, unit_index, function.generic_parameters);
    for(auto const& parameter : function.parameters) {
        if(parameter.type) {
            collect_type_navigation(collector, ast, checked, unit_index, *parameter.type);
        }
        if(parameter.default_value) {
            collect_expression_navigation(collector, ast, checked, unit_index, *parameter.default_value);
        }
    }
    if(function.return_type) {
        collect_type_navigation(collector, ast, checked, unit_index, *function.return_type);
    }
    if(function.has_body) {
        collect_statement_navigation(collector, ast, checked, unit_index, function.body);
    }
}

auto collect_statement_navigation(navigation_collector& collector, ast_arena const& ast, semantic_result const& checked, std::size_t unit_index, stmt_id id) -> void
{
    auto const& statement = ast.node(id);
    std::visit(overloaded {
        [&](block_statement_syntax const& node) {
            for(auto child : node.statements) {
                collect_statement_navigation(collector, ast, checked, unit_index, child);
            }
        },
        [&](declaration_statement_syntax const& node) {
            if(node.declared_type) {
                collect_type_navigation(collector, ast, checked, unit_index, *node.declared_type);
            }
            collect_expression_navigation(collector, ast, checked, unit_index, node.initializer);
        },
        [&](type_alias_statement_syntax const& node) {
            collect_type_navigation(collector, ast, checked, unit_index, node.type);
        },
        [&](if_statement_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.condition);
            collect_statement_navigation(collector, ast, checked, unit_index, node.then_branch);
            if(node.else_branch) {
                collect_statement_navigation(collector, ast, checked, unit_index, *node.else_branch);
            }
        },
        [&](while_statement_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.condition);
            collect_statement_navigation(collector, ast, checked, unit_index, node.body);
        },
        [&](do_while_statement_syntax const& node) {
            collect_statement_navigation(collector, ast, checked, unit_index, node.body);
            collect_expression_navigation(collector, ast, checked, unit_index, node.condition);
        },
        [&](for_statement_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.range);
            collect_statement_navigation(collector, ast, checked, unit_index, node.body);
        },
        [&](template_for_statement_syntax const&) {},
        [&](template_if_statement_syntax const& node) {
            for(auto const& condition : node.conditions) {
                if(condition.expression) {
                    collect_expression_navigation(collector, ast, checked, unit_index, *condition.expression);
                }
            }
            for(auto const& branch : node.branches) {
                collect_statement_navigation(collector, ast, checked, unit_index, branch.body);
            }
            if(node.else_branch) {
                collect_statement_navigation(collector, ast, checked, unit_index, *node.else_branch);
            }
        },
        [](break_statement_syntax const&) {},
        [](continue_statement_syntax const&) {},
        [&](return_statement_syntax const& node) {
            if(node.value) {
                collect_expression_navigation(collector, ast, checked, unit_index, *node.value);
            }
        },
        [&](expression_statement_syntax const& node) {
            collect_expression_navigation(collector, ast, checked, unit_index, node.expression);
        },
    }, statement);
}

auto collect_ast_navigation(navigation_collector& collector, parse_result const& parsed, semantic_result const& checked, std::size_t unit_index) -> void
{
    if(not parsed.root) {
        return;
    }

    auto const& root = *parsed.root;
    for(auto id : root.structs) {
        auto const& value = parsed.ast.node(id);
        collect_generic_parameter_navigation(collector, parsed.ast, checked, unit_index, value.generic_parameters);
        for(auto const& field : value.fields) {
            collect_type_navigation(collector, parsed.ast, checked, unit_index, field.type);
            if(field.default_value) {
                collect_expression_navigation(collector, parsed.ast, checked, unit_index, *field.default_value);
            }
        }
    }
    for(auto id : root.enums) {
        auto const& value = parsed.ast.node(id);
        collect_type_navigation(collector, parsed.ast, checked, unit_index, value.underlying_type);
        for(auto const& enum_case : value.cases) {
            collect_expression_navigation(collector, parsed.ast, checked, unit_index, enum_case.value);
        }
    }
    for(auto id : root.variants) {
        auto const& value = parsed.ast.node(id);
        collect_generic_parameter_navigation(collector, parsed.ast, checked, unit_index, value.generic_parameters);
        for(auto const& variant_case : value.cases) {
            for(auto payload : variant_case.payloads) {
                collect_type_navigation(collector, parsed.ast, checked, unit_index, payload);
            }
        }
    }
    for(auto id : root.concepts) {
        auto const& value = parsed.ast.node(id);
        collect_generic_parameter_navigation(collector, parsed.ast, checked, unit_index, value.generic_parameters);
        for(auto const& item : value.items) {
            std::visit(overloaded {
                [&](concept_requires_syntax const& requirement) {
                    for(auto const& constraint : requirement.constraints) {
                        std::visit(overloaded {
                            [&](concept_parent_constraint_syntax const& parent) {
                                collect_concept_id_navigation(collector, parsed.ast, checked, unit_index, parent.parent);
                            },
                            [&](concept_type_bound_constraint_syntax const& bound) {
                                collect_type_navigation(collector, parsed.ast, checked, unit_index, bound.type);
                                for(auto const& concept_bound : bound.concept_bounds) {
                                    collect_concept_id_navigation(collector, parsed.ast, checked, unit_index, concept_bound);
                                }
                            },
                            [&](concept_type_equality_constraint_syntax const& equality) {
                                collect_type_navigation(collector, parsed.ast, checked, unit_index, equality.left);
                                collect_type_navigation(collector, parsed.ast, checked, unit_index, equality.right);
                            },
                        }, constraint);
                    }
                },
                [&](type_alias_syntax const& alias) {
                    if(alias.value) {
                        collect_type_navigation(collector, parsed.ast, checked, unit_index, *alias.value);
                    }
                },
                [&](concept_function_requirement_syntax const& function) {
                    if(function.default_function) {
                        collect_function_navigation(collector, parsed.ast, checked, unit_index, parsed.ast.node(*function.default_function));
                        return;
                    }
                    for(auto const& parameter : function.parameters) {
                        if(parameter.type) {
                            collect_type_navigation(collector, parsed.ast, checked, unit_index, *parameter.type);
                        }
                        if(parameter.default_value) {
                            collect_expression_navigation(collector, parsed.ast, checked, unit_index, *parameter.default_value);
                        }
                    }
                    if(function.return_type) {
                        collect_type_navigation(collector, parsed.ast, checked, unit_index, *function.return_type);
                    }
                },
            }, item);
        }
    }
    for(auto id : root.type_aliases) {
        auto const& value = parsed.ast.node(id);
        if(value.value) {
            collect_type_navigation(collector, parsed.ast, checked, unit_index, *value.value);
        }
    }
    for(auto id : root.impls) {
        auto const& value = parsed.ast.node(id);
        collect_generic_parameter_navigation(collector, parsed.ast, checked, unit_index, value.generic_parameters);
        collect_type_navigation(collector, parsed.ast, checked, unit_index, value.type);
        for(auto const& alias : value.type_aliases) {
            if(alias.value) {
                collect_type_navigation(collector, parsed.ast, checked, unit_index, *alias.value);
            }
        }
        for(auto function_id : value.functions) {
            collect_function_navigation(collector, parsed.ast, checked, unit_index, parsed.ast.node(function_id));
        }
    }
    for(auto id : root.concept_impls) {
        auto const& value = parsed.ast.node(id);
        collect_generic_parameter_navigation(collector, parsed.ast, checked, unit_index, value.generic_parameters);
        collect_concept_id_navigation(collector, parsed.ast, checked, unit_index, value.concept_name);
        collect_type_navigation(collector, parsed.ast, checked, unit_index, value.target_type);
        for(auto const& alias : value.type_aliases) {
            if(alias.value) {
                collect_type_navigation(collector, parsed.ast, checked, unit_index, *alias.value);
            }
        }
        for(auto function_id : value.functions) {
            collect_function_navigation(collector, parsed.ast, checked, unit_index, parsed.ast.node(function_id));
        }
    }
    for(auto id : root.functions) {
        collect_function_navigation(collector, parsed.ast, checked, unit_index, parsed.ast.node(id));
    }
}

auto parsed_unit_index_for_file(std::span<file_id const> parsed_file_ids, file_id file) -> std::optional<std::size_t>
{
    auto found = std::ranges::find(parsed_file_ids, file);
    if(found == parsed_file_ids.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(parsed_file_ids.begin(), found));
}

auto unit_imports_module(source_manager const& sources, parse_result const& parsed, std::string_view module_name) -> bool
{
    if(not parsed.root) {
        return false;
    }

    return std::ranges::any_of(parsed.root->imports, [&](import_syntax const& import) {
        return module_name_text(sources, import.name) == module_name;
    });
}

auto active_module_name(source_manager const& sources, parse_result const& parsed) -> std::optional<std::string>
{
    if(not parsed.root or not parsed.root->module_header) {
        return std::nullopt;
    }
    return module_name_text(sources, parsed.root->module_header->name);
}

auto non_exported_free_functions(source_manager const& sources, parse_result const& parsed) -> std::map<std::string, source_span>
{
    auto result = std::map<std::string, source_span>{};
    if(not parsed.root) {
        return result;
    }

    for(auto id : parsed.root->functions) {
        auto const& function = parsed.ast.node(id);
        if(function.exported or function.kind == function_syntax_kind::lambda) {
            continue;
        }
        result.emplace(std::string{sources.slice(function.name)}, function.name);
    }
    return result;
}

auto make_unexported_declaration_record(source_manager const& sources, byte_pos active_start, source_span declaration, std::string_view name, std::string_view module_name) -> diagnostic_record
{
    auto const position = sources.position(declaration.start);
    return diagnostic_record {
        .stage = "semantic",
        .code = "unexported_name",
        .message = std::format("function '{}' is used through import '{}' but is not exported", name, module_name),
        .severity = "error",
        .start_offset = declaration.start - active_start,
        .end_offset = declaration.end - active_start,
        .line = position.line,
        .column = position.column,
    };
}

auto append_unexported_declaration_diagnostics(std::vector<diagnostic_record>& output, source_manager const& sources, byte_pos active_start, file_id active_file, std::span<parse_result const> parsed_units, std::span<file_id const> parsed_file_ids, std::size_t active_unit, std::span<diagnostic const> diagnostics) -> void
{
    auto const module_name = active_module_name(sources, parsed_units[active_unit]);
    if(not module_name) {
        return;
    }

    auto declarations = non_exported_free_functions(sources, parsed_units[active_unit]);
    if(declarations.empty()) {
        return;
    }

    auto emitted = std::set<std::string>{};
    for(auto const& diagnostic : diagnostics) {
        if(diagnostic.kind != diagnostic_kind::unknown_name and diagnostic.kind != diagnostic_kind::unexported_name) {
            continue;
        }

        auto const use_file = span_file(sources, diagnostic.primary_span);
        if(use_file == active_file) {
            continue;
        }

        auto use_unit = parsed_unit_index_for_file(parsed_file_ids, use_file);
        if(not use_unit or not unit_imports_module(sources, parsed_units[*use_unit], *module_name)) {
            continue;
        }

        auto name = std::string{sources.slice(diagnostic.primary_span)};
        auto declaration = declarations.find(name);
        if(declaration == declarations.end() or not emitted.emplace(name).second) {
            continue;
        }

        output.emplace_back(make_unexported_declaration_record(sources, active_start, declaration->second, name, *module_name));
    }
}

auto read_all(std::istream& input) -> std::string
{
    return std::string {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

auto parse_source_files(nlohmann::ordered_json const& payload, std::ostream& error, bool require_field)
    -> std::optional<std::vector<source_file_record>>
{
    auto result = std::vector<source_file_record>{};
    auto const files = payload.find("files");
    if(files == payload.end()) {
        if(require_field) {
            error << "request must contain files array\n";
            return std::nullopt;
        }
        return result;
    }
    if(not files->is_array()) {
        error << "files must be an array\n";
        return std::nullopt;
    }

    for(auto const& file : *files) {
        if(not file.is_object()) {
            error << "file entry must be an object\n";
            return std::nullopt;
        }
        result.emplace_back(
            file.value("path", std::string{}),
            file.value("text", std::string{})
        );
    }
    return result;
}

auto parse_string_array(nlohmann::ordered_json const& payload, std::string_view field, std::ostream& error)
    -> std::optional<std::vector<std::string>>
{
    auto result = std::vector<std::string>{};
    auto const array = payload.find(field);
    if(array == payload.end()) {
        return result;
    }
    if(not array->is_array()) {
        error << field << " must be an array\n";
        return std::nullopt;
    }
    for(auto const& item : *array) {
        if(not item.is_string()) {
            error << field << " entries must be strings\n";
            return std::nullopt;
        }
        result.emplace_back(item.get<std::string>());
    }
    return result;
}

auto parse_inspect_request(std::string_view text, std::ostream& error) -> std::optional<inspect_request>
{
    try {
        auto const payload = nlohmann::ordered_json::parse(text);
        auto request = inspect_request{};
        request.active_file = payload.value("activeFile", std::string{});
        auto files = parse_source_files(payload, error, true);
        if(not files) {
            return std::nullopt;
        }
        request.files = std::move(*files);
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

auto parse_resolve_request(std::string_view text, std::ostream& error) -> std::optional<resolve_request>
{
    try {
        auto const payload = nlohmann::ordered_json::parse(text);
        auto request = resolve_request{};
        request.active_file = payload.value("activeFile", std::string{});
        request.target_file = payload.value("targetFile", std::string{});
        request.follow_stdlib_imports = payload.value("followStdlibImports", true);

        auto entry_files = parse_string_array(payload, "entryFiles", error);
        auto import_roots = parse_string_array(payload, "importRoots", error);
        auto search_roots = parse_string_array(payload, "searchRoots", error);
        auto files = parse_source_files(payload, error, false);
        if(not entry_files or not import_roots or not search_roots or not files) {
            return std::nullopt;
        }
        request.entry_files = std::move(*entry_files);
        request.import_roots = std::move(*import_roots);
        request.search_roots = std::move(*search_roots);
        request.files = std::move(*files);

        if(request.active_file.empty()) {
            error << "resolve request must contain activeFile\n";
            return std::nullopt;
        }
        return request;
    } catch(nlohmann::json::exception const& exception) {
        error << "invalid resolve json: " << exception.what() << '\n';
        return std::nullopt;
    }
}

auto usage(std::ostream& error) -> void
{
    error << "usage: kcp-lexer-helper <analyze|tokens|inspect|resolve> --stdin [--filename <name>] --format json\n";
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
        or request.command == "resolve"
    );
    auto const needs_filename = request.command != "inspect" and request.command != "resolve";

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

auto resolve(resolve_request request) -> inspect_request
{
    auto native = cp_imports::resolve_request {
        .active_file = std::move(request.active_file),
        .target_file = std::move(request.target_file),
        .entry_files = std::move(request.entry_files),
        .import_roots = std::move(request.import_roots),
        .search_roots = std::move(request.search_roots),
        .follow_stdlib_imports = request.follow_stdlib_imports,
    };
    native.files.reserve(request.files.size());
    for(auto& file : request.files) {
        native.files.emplace_back(
            cp_imports::source_file {
                .path = std::move(file.path),
                .text = std::move(file.text),
            }
        );
    }

    auto resolved = cp_imports::resolve_source_files(std::move(native));
    auto output = inspect_request {
        .active_file = std::move(resolved.active_file),
    };
    output.files.reserve(resolved.files.size());
    for(auto& file : resolved.files) {
        output.files.emplace_back(
            std::move(file.path),
            std::move(file.text)
        );
    }
    return output;
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
    auto parsed_file_ids = std::vector<file_id>{};
    auto active_unit = std::optional<std::size_t>{};
    auto active_tokens = std::vector<token>{};
    auto frontend_ok = true;

    for(auto index = 0uz; index < file_ids.size(); ++index) {
        auto const file = file_ids[index];
        auto preprocessed = preprocess(sources, file);
        if(contains_error_diagnostic(std::span{ preprocessed.diagnostics })) {
            frontend_ok = false;
            append_diagnostics(all_diagnostics, preprocessed.diagnostics);
            continue;
        }

        auto lexical = lex(preprocessed);
        if(file == *active_file) {
            active_tokens = lexical.tokens;
        }
        if(contains_error_diagnostic(std::span{ lexical.diagnostics })) {
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
        parsed_file_ids.emplace_back(file);
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
    if(active_unit) {
        append_unexported_declaration_diagnostics(
            result.diagnostics,
            sources,
            active_start,
            *active_file,
            std::span<parse_result const>{ parsed_units.data(), parsed_units.size() },
            std::span<file_id const>{ parsed_file_ids.data(), parsed_file_ids.size() },
            *active_unit,
            std::span<diagnostic const>{ all_diagnostics.data(), all_diagnostics.size() }
        );
    }

    auto collector = highlight_collector{ sources, *active_file };
    add_token_highlights(collector, active_tokens);
    if(active_unit) {
        auto const semantic = checked ? &*checked : nullptr;
        collect_ast_highlights(collector, parsed_units[*active_unit], semantic, *active_unit);
    }
    result.highlights = std::move(collector.highlights);
    result.captures = std::move(collector.captures);

    if(active_unit and checked) {
        auto navigation = navigation_collector{ sources, *active_file };
        collect_import_navigation(
            navigation,
            std::span<parse_result const>{ parsed_units.data(), parsed_units.size() },
            *active_unit
        );
        collect_ast_navigation(navigation, parsed_units[*active_unit], *checked, *active_unit);
        result.navigation = std::move(navigation.navigation);
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

auto inspect_request_to_json(inspect_request const& request) -> std::string
{
    auto payload = nlohmann::ordered_json{};
    payload["activeFile"] = request.active_file;
    payload["files"] = nlohmann::ordered_json::array();
    for(auto const& file : request.files) {
        auto& item = payload["files"].emplace_back();
        item["path"] = file.path;
        item["text"] = file.text;
    }
    return payload.dump();
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

    payload["captures"] = nlohmann::ordered_json::array();
    for(auto const& capture : result.captures) {
        auto& item = payload["captures"].emplace_back();
        item["name"] = capture.name;
        item["mode"] = capture.mode;
        item["reason"] = capture.reason;
        item["referenceStartOffset"] = capture.reference_start_offset;
        item["referenceEndOffset"] = capture.reference_end_offset;
        item["sourceStartOffset"] = capture.source_start_offset;
        item["sourceEndOffset"] = capture.source_end_offset;
        item["mutated"] = capture.mutated;
        item["escaped"] = capture.escaped;
    }

    payload["navigation"] = nlohmann::ordered_json::array();
    for(auto const& navigation : result.navigation) {
        auto& item = payload["navigation"].emplace_back();
        item["category"] = navigation.category;
        item["sourceStartOffset"] = navigation.source_start_offset;
        item["sourceEndOffset"] = navigation.source_end_offset;
        item["targetFile"] = navigation.target_file;
        item["targetStartOffset"] = navigation.target_start_offset;
        item["targetEndOffset"] = navigation.target_end_offset;
    }

    return payload.dump();
}

auto run_cli(std::span<std::string_view const> args, std::istream& input, std::ostream& output, std::ostream& error) -> int
{
    auto const request = parse_cli(args, error);
    if(not request) {
        return 2;
    }

    auto const text = read_all(input);
    if(request->command == "resolve") {
        auto parsed = parse_resolve_request(text, error);
        if(not parsed) {
            return 2;
        }
        output << inspect_request_to_json(resolve(std::move(*parsed)));
        return 0;
    }

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
