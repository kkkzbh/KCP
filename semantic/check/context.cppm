module semantic.check:context;

import std;
import source;
import parser;
import semantic.ty;
import semantic.symbol;
import semantic.diagnostic;
import semantic.result;
import semantic.check.keys;
import :state;

struct semantic_context
{
    explicit semantic_context(source_manager const& sources) :
        ast_source(sources) {}

    auto bind_symbol(semantic_symbol symbol) -> symbol_id
    {
        auto& scope = scopes.back();
        if(scope.contains(symbol.name)) {
            report (
                semantic_diagnostic_code::duplicate_symbol,
                symbol.span,
                std::format("duplicate symbol '{}'", symbol.name)
            );
            return {};
        }

        auto id = add_symbol(std::move(symbol));
        scope.emplace(result.symbols[id.value].name, id);
        return id;
    }

    auto resolve(std::string_view name) const -> symbol_id
    {
        for(auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
            if(auto found = scope->find(std::string{ name }); found != scope->end()) {
                return found->second;
            }
        }
        return {};
    }

    auto add_symbol(semantic_symbol symbol) -> symbol_id
    {
        auto id = symbol_id{ static_cast<std::uint32_t>(result.symbols.size()) };
        result.symbols.emplace_back(std::move(symbol));
        return id;
    }

    auto add_signature(function_signature signature) -> function_signature_id
    {
        auto id = function_signature_id{ static_cast<std::uint32_t>(result.signatures.size()) };
        result.signatures.emplace_back(std::move(signature));
        return id;
    }

    auto report(semantic_diagnostic_code code, source_span span, std::string message) -> void
    {
        result.diagnostics.emplace_back (
            code,
            span,
            std::move(message)
        );
    }

    ast_source_view ast_source;
    semantic_result result{};
    std::vector<unit_state> units{};
    std::map<std::string, std::map<std::string, symbol_id>> module_functions{};
    std::map<std::string, std::map<std::string, symbol_id>> module_exports{};
    std::vector<std::map<std::string, symbol_id>> scopes{};
    std::vector<loop_label> loops{};
    std::map<parameter_binding_key, symbol_id> parameter_symbols{};
    std::size_t current_unit{};
};
