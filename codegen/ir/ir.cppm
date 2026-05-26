export module codegen.ir;

import std;
import source;
import lexer.token;
import parser;
import semantic;

export struct [[nodiscard]] ir_value_id
{
    constexpr ir_value_id() = default;

    explicit constexpr ir_value_id(std::uint32_t raw) :
        value(raw) {}

    auto constexpr valid() const -> bool
    {
        return value != invalid_value;
    }

    auto constexpr operator==(ir_value_id const&) const -> bool = default;
    auto constexpr operator<=>(ir_value_id const&) const = default;

    auto constexpr static invalid_value = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t value{ invalid_value };
};

export struct [[nodiscard]] ir_block_id
{
    constexpr ir_block_id() = default;

    explicit constexpr ir_block_id(std::uint32_t raw) :
        value(raw) {}

    auto constexpr valid() const -> bool
    {
        return value != invalid_value;
    }

    auto constexpr operator==(ir_block_id const&) const -> bool = default;
    auto constexpr operator<=>(ir_block_id const&) const = default;

    auto constexpr static invalid_value = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t value{ invalid_value };
};

export enum class ir_linkage : std::uint8_t
{
    internal,
    external,
};

export enum class ir_opcode : std::uint8_t
{
    alloca_,
    literal,
    load,
    store,
    field_address,
    element_address,
    unary,
    binary,
    cast,
    aggregate_undef,
    insert_value,
    extract_value,
    alloc_raw,
    free_raw,
    call,
    panic,
    branch,
    cond_branch,
    return_,
};

export struct ir_parameter
{
    ir_parameter() = default;

    ir_parameter(ir_value_id parameter_value, semantic_type_id parameter_type, symbol_id parameter_symbol, std::string parameter_name) :
        value(parameter_value),
        type(parameter_type),
        symbol(parameter_symbol),
        name(std::move(parameter_name)) {}

    ir_value_id value{};
    semantic_type_id type{};
    symbol_id symbol{};
    std::string name{};
};

export struct ir_instruction
{
    auto has_result() const -> bool
    {
        return result.valid();
    }

    ir_opcode opcode{};
    ir_value_id result{};
    semantic_type_id type{};
    std::vector<ir_value_id> operands{};
    std::vector<ir_block_id> targets{};
    std::vector<std::uint64_t> indices{};
    token_kind operator_kind{ token_kind::eof };
    semantic_type_id aggregate_type{};
    symbol_id symbol{};
    semantic_literal_value literal{};
    std::string name{};
};

export struct ir_block
{
    ir_block() = default;

    explicit ir_block(std::string block_name) :
        name(std::move(block_name)) {}

    auto terminated() const -> bool
    {
        if(instructions.empty()) {
            return false;
        }
        auto opcode = instructions.back().opcode;
        return opcode == ir_opcode::branch
               or opcode == ir_opcode::cond_branch
               or (opcode == ir_opcode::call and is_never(instructions.back().type))
               or opcode == ir_opcode::panic
               or opcode == ir_opcode::return_;
    }

    std::string name{};
    std::vector<ir_instruction> instructions{};
};

export struct ir_function
{
    ir_function() = default;

    ir_function(std::string function_name, ir_linkage function_linkage, semantic_type_id return_type, symbol_id function_symbol) :
        name(std::move(function_name)),
        linkage(function_linkage),
        returns(return_type),
        symbol(function_symbol) {}

    std::string name{};
    ir_linkage linkage{};
    semantic_type_id returns{};
    symbol_id symbol{};
    std::vector<ir_parameter> parameters{};
    std::vector<ir_block> blocks{};
};

export struct ir_module
{
    type_arena types{};
    std::vector<semantic_struct> structs{};
    std::vector<semantic_enum> enums{};
    std::vector<semantic_opaque_alias> opaque_aliases{};
    std::vector<semantic_variant> variants{};
    std::vector<ir_function> functions{};
};

export struct ir_emit_result
{
    bool accepted{};
    ir_module module{};
    std::string error{};
};

export struct ir_emit_options
{
    bool elide_asserts{};
};

struct function_lowerer
{
    struct lowerer_state
    {
        std::size_t unit_index{};
        parse_result const* parsed{};
        std::vector<std::size_t> context_stack{};
    };

    enum class field_default_initialization_state
    {
        absent,
        present,
        deferred,
    };

    using address_binding_state = std::pair<symbol_id, std::optional<ir_value_id>>;

    function_lowerer(source_manager const& sources, std::span<parse_result const> units, semantic_result const& semantics, ir_module& module, ir_emit_options options, std::size_t unit_index, function_id id, std::size_t context_index = 0uz) :
        ast_source(sources),
        units(units),
        parsed(&units[unit_index]),
        semantics(semantics),
        module(module),
        options(options),
        unit_index(unit_index),
        function_id_value(id),
        context_index(context_index) {}

    auto signature_of(function_id id) const -> function_signature_id
    {
        return semantics.signature_of(current_context_index(), unit_index, id);
    }

    auto function_symbol_of(function_id id) const -> symbol_id
    {
        return semantics.function_symbol_of(current_context_index(), unit_index, id);
    }

    auto binding_of(stmt_id id) const -> symbol_id
    {
        return semantics.binding_of(current_context_index(), unit_index, id);
    }

    auto for_range_of(stmt_id id) const -> semantic_for_range_info
    {
        return semantics.for_range_of(current_context_index(), unit_index, id);
    }

    auto template_for_expansions_of(stmt_id id) const -> std::vector<semantic_template_for_expansion>
    {
        return semantics.template_for_expansions_of(current_context_index(), unit_index, id);
    }

    auto local_binding_of(source_span name) const -> symbol_id
    {
        return semantics.local_binding_of(current_context_index(), unit_index, name);
    }

    auto parameter_binding_of(source_span name) const -> symbol_id
    {
        return semantics.parameter_binding_of(current_context_index(), unit_index, name);
    }

    auto parameter_pack_bindings_of(source_span name) const -> std::vector<symbol_id>
    {
        return semantics.parameter_pack_bindings_of(current_context_index(), unit_index, name);
    }

    auto info_of(expr_id id) const -> semantic_expression_info
    {
        return semantics.info_of(current_context_index(), unit_index, id);
    }

    auto conversion_of(expr_id id) const -> semantic_type_id
    {
        return semantics.conversion_of(current_context_index(), unit_index, id);
    }

    auto literal_of(expr_id id) const -> semantic_literal_value
    {
        return semantics.literal_of(current_context_index(), unit_index, id);
    }

    auto builtin_call_of(expr_id id) const -> semantic_builtin_call
    {
        return semantics.builtin_call_of(current_context_index(), unit_index, id);
    }

    auto field_access_of(expr_id id) const -> semantic_field_access
    {
        return semantics.field_access_of(current_context_index(), unit_index, id);
    }

    auto variant_case_of(expr_id id) const -> semantic_variant_case_access
    {
        return semantics.variant_case_of(current_context_index(), unit_index, id);
    }

    auto lambda_of(function_id id) const -> semantic_lambda_info
    {
        return semantics.lambda_of(current_context_index(), unit_index, id);
    }

    auto lambda_capture_of(expr_id id) const -> semantic_lambda_capture_access
    {
        return semantics.lambda_capture_of(current_context_index(), unit_index, id);
    }

    auto lambda_call_of(expr_id id) const -> semantic_lambda_info
    {
        return semantics.lambda_call_of(current_context_index(), unit_index, id);
    }

    auto pattern_binding_of(source_span name) const -> symbol_id
    {
        return semantics.pattern_binding_of(current_context_index(), unit_index, name);
    }

    auto resolved_name(expr_id id) const -> symbol_id
    {
        return semantics.resolved_name(current_context_index(), unit_index, id);
    }

    auto selected_operator(expr_id id) const -> symbol_id
    {
        return semantics.selected_operator(current_context_index(), unit_index, id);
    }

    auto current_context_index() const -> std::size_t
    {
        return context_stack.empty() ? context_index : context_stack.back();
    }

    auto save_lowerer_state() const -> lowerer_state
    {
        return lowerer_state {
            .unit_index = unit_index,
            .parsed = parsed,
            .context_stack = context_stack,
        };
    }

    auto enter_lowerer_state(std::size_t next_unit, std::size_t next_context) -> void
    {
        unit_index = next_unit;
        parsed = &units[unit_index];
        context_stack = { next_context };
    }

    auto restore_lowerer_state(lowerer_state state) -> void
    {
        unit_index = state.unit_index;
        parsed = state.parsed;
        context_stack = std::move(state.context_stack);
    }

    auto lower() -> bool
    {
        auto const& syntax = parsed->ast.node(function_id_value);
        auto signature_id = signature_of(function_id_value);
        auto symbol = function_symbol_of(function_id_value);
        if(not signature_id.valid() or not symbol.valid()) {
            return fail("function is missing semantic signature or symbol");
        }
        auto const& symbol_info = semantics.symbols[symbol.value];

        auto lambda = lambda_of(function_id_value);
        auto return_type = lambda.valid()
            ? lambda.callable.returns
            : semantics.signatures[signature_id.value].returns;
        module.functions.emplace_back(
            lowered_function_name(symbol),
            lowered_function_linkage(syntax),
            return_type,
            symbol
        );
        function = &module.functions.back();

        auto const& signature = semantics.signatures[signature_id.value];
        if(not semantic_function_body_has_source(symbol_info.body_kind)) {
            for(auto index = 0uz; index < syntax.parameters.size() and index < signature.parameters.size(); ++index) {
                function->parameters.emplace_back(
                    make_value(),
                    signature.parameters[index],
                    symbol_id{},
                    std::string{ ast_source.slice(syntax.parameters[index].name) }
                );
            }
            return true;
        }

        current = add_block("entry");
        auto parameter_index = 0uz;
        if(lambda.valid() and lambda.env_symbol.valid()) {
            bind_parameter(lambda.env_symbol, semantics.symbols[lambda.env_symbol.value].type, "closure.env");
        }
        if(semantics.symbols[symbol.value].function_kind == semantic_function_kind::destructor) {
            auto self = implicit_destructor_self_symbol();
            if(not self.valid()) {
                return fail("destructor is missing implicit self binding");
            }
            bind_parameter(self, signature.parameters[parameter_index++], "self");
        }

        for(auto index = 0uz; index < syntax.parameters.size(); ++index) {
            auto const& parameter = syntax.parameters[index];
            if(parameter.is_pack) {
                auto parameter_symbols = parameter_pack_bindings_of(parameter.name);
                for(auto pack_index = 0uz; pack_index < parameter_symbols.size(); ++pack_index) {
                    bind_parameter(
                        parameter_symbols[pack_index],
                        signature.parameters[parameter_index++],
                        std::format("{}.{}", ast_source.slice(parameter.name), pack_index)
                    );
                }
                continue;
            }
            auto parameter_symbol = parameter_binding_of(parameter.name);
            if(not parameter_symbol.valid()) {
                return fail("parameter is missing semantic binding");
            }

            bind_parameter(
                parameter_symbol,
                signature.parameters[parameter_index++],
                std::string{ ast_source.slice(parameter.name) }
            );
        }

        if(not emit_statement(syntax.body)) {
            return false;
        }
        if(not current_block().terminated()) {
            emit_cleanups_to(0uz);
            emit_default_return(signature.returns);
        }
        return true;
    }

    auto bind_parameter(symbol_id parameter_symbol, semantic_type_id type, std::string name) -> void
    {
        auto value = make_value();
        function->parameters.emplace_back(value, type, parameter_symbol, name);
        auto address = emit_alloca(type, function->parameters.back().name);
        bind(parameter_symbol, address);
        emit_store(address, value, type);
        if(not is_reference(type)) {
            register_cleanup(parameter_symbol, address, type);
        }
    }

    auto implicit_destructor_self_symbol() const -> symbol_id
    {
        for(auto index = 0uz; index < semantics.symbols.size(); ++index) {
            auto const& symbol = semantics.symbols[index];
            if(symbol.kind == symbol_kind::parameter
               and symbol.name == "self"
               and symbol.unit_index == unit_index
               and symbol.function == function_id_value) {
                return symbol_id{ static_cast<std::uint32_t>(index) };
            }
        }
        return {};
    }

    auto lowered_function_name(symbol_id symbol) -> std::string
    {
        auto const& value = semantics.symbols[symbol.value];
        auto module_name = parsed_module_name();
        if(value.function_kind != semantic_function_kind::free_function) {
            auto owner_module = module_name.empty() ? "local" : module_name;
            auto owner_name = (
                value.struct_index != std::numeric_limits<std::uint32_t>::max()
                    ? module.structs[value.struct_index].name
                    : (
                        value.variant_index != std::numeric_limits<std::uint32_t>::max()
                            ? module.variants[value.variant_index].name
                            : (
                                value.opaque_index != std::numeric_limits<std::uint32_t>::max()
                                    ? module.opaque_aliases[value.opaque_index].name
                                    : (
                                        as_builtin(value.owner_type)
                                            ? std::string{ type_name(*as_builtin(value.owner_type)) }
                                            : "array"
                                    )
                            )
                    )
            );
            return std::format("cp.{}.{}.{}.{}", owner_module, owner_name, value.name, symbol.value);
        }
        if(module_name.empty() and value.name == "main") {
            return "main";
        }
        if(parsed->ast.node(function_id_value).extern_abi) {
            return value.name;
        }
        if(module_name.empty()) {
            return value.name;
        }
        return std::format("cp.{}.{}", module_name, value.name);
    }

    auto lowered_function_linkage(function_syntax const& syntax) -> ir_linkage
    {
        auto symbol = function_symbol_of(function_id_value);
        if(context_index != 0uz) {
            return ir_linkage::internal;
        }
        if(symbol.valid() and semantic_function_body_is_extern_declaration(semantics.symbols[symbol.value].body_kind)) {
            return ir_linkage::external;
        }
        if(symbol.valid() and semantics.symbols[symbol.value].function_kind != semantic_function_kind::free_function) {
            return ir_linkage::internal;
        }
        if(syntax.extern_abi) {
            return ir_linkage::external;
        }
        auto name = ast_source.slice(syntax.name);
        if(parsed_module_name().empty() and name == "main") {
            return ir_linkage::external;
        }
        return syntax.exported ? ir_linkage::external : ir_linkage::internal;
    }

    auto parsed_module_name() -> std::string
    {
        if(not parsed->root or not parsed->root->module_header) {
            return {};
        }
        return ast_source.module_name(parsed->root->module_header->name);
    }

    auto add_block(std::string name) -> ir_block_id
    {
        auto id = ir_block_id{ static_cast<std::uint32_t>(function->blocks.size()) };
        function->blocks.emplace_back(std::move(name));
        return id;
    }

    auto current_block() -> ir_block&
    {
        return function->blocks[current.value];
    }

    auto make_value() -> ir_value_id
    {
        return ir_value_id{ next_value++ };
    }

    auto emit(ir_instruction instruction) -> ir_value_id
    {
        auto result = instruction.result;
        current_block().instructions.emplace_back(std::move(instruction));
        return result;
    }

    auto emit_void(ir_instruction instruction) -> void
    {
        current_block().instructions.emplace_back(std::move(instruction));
    }

    auto emit_value_instruction(ir_opcode opcode, semantic_type_id type) -> ir_instruction
    {
        return ir_instruction {
            .opcode = opcode,
            .result = make_value(),
            .type = type,
        };
    }

    auto emit_alloca(semantic_type_id type, std::string name) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::alloca_, type);
        instruction.name = std::move(name);
        return emit(std::move(instruction));
    }

    auto emit_store(ir_value_id address, ir_value_id value, semantic_type_id type) -> void
    {
        emit_void(ir_instruction {
            .opcode = ir_opcode::store,
            .type = type,
            .operands = { address, value },
        });
    }

    auto emit_load(ir_value_id address, semantic_type_id type) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::load, type);
        instruction.operands = { address };
        return emit(std::move(instruction));
    }

    auto emit_field_address(ir_value_id base, semantic_type_id aggregate_type, semantic_type_id field_type, std::uint64_t index) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::field_address, field_type);
        instruction.operands = { base };
        instruction.aggregate_type = aggregate_type;
        instruction.indices = { index };
        return emit(std::move(instruction));
    }

    auto emit_field_value_address(ir_value_id base, semantic_type_id aggregate_type, semantic_field_access field) -> ir_value_id
    {
        auto field_type = struct_field_type(aggregate_type, field);
        auto address = emit_field_address(base, aggregate_type, field_type, field.field_index);
        if(is_reference(field_type)) {
            return emit_load(address, field_type);
        }
        return address;
    }

    auto expression_requests_move(expr_id id) const -> bool
    {
        auto const& expression = parsed->ast.node(stripped_expression(id));
        auto const* unary = std::get_if<unary_expr_syntax>(&expression);
        return unary != nullptr
               and (unary->operator_kind == token_kind::kw_move or unary->operator_kind == token_kind::kw_forward);
    }

    auto copy_move_constructor_for(expr_id argument, semantic_type_id target_type) const -> symbol_id
    {
        auto info = info_of(argument);
        if(read_type(info.type) != read_type(target_type) or not info.is_lvalue) {
            return {};
        }
        auto struct_index = struct_index_of(target_type);
        if(not struct_index) {
            return {};
        }

        auto wants_move = expression_requests_move(argument);
        for(auto symbol : module.structs[*struct_index].constructors) {
            auto const& constructor = semantics.symbols[symbol.value];
            if(semantic_function_body_is_deleted(constructor.body_kind)) {
                continue;
            }
            auto const* function = std::get_if<function_type>(&module.types.get(constructor.type));
            if(function == nullptr or function->parameters.size() != 1uz) {
                continue;
            }
            auto const* reference = std::get_if<reference_type>(&module.types.get(function->parameters.front()));
            if(reference == nullptr or read_type(reference->pointee) != read_type(target_type)) {
                continue;
            }
            if(wants_move) {
                if(reference->reference_kind == reference_type::kind::move and not reference->is_const) {
                    return symbol;
                }
                continue;
            }
            if(reference->reference_kind == reference_type::kind::regular and (reference->is_const or not info.is_const)) {
                return symbol;
            }
        }
        return {};
    }

    auto emit_constructed_value(expr_id argument, semantic_type_id target_type) -> ir_value_id
    {
        if(auto constructor = copy_move_constructor_for(argument, target_type); constructor.valid()) {
            auto const* function = std::get_if<function_type>(&module.types.get(semantics.symbols[constructor.value].type));
            if(function == nullptr or function->parameters.empty()) {
                return unsupported_expression("copy/move constructor is missing function type");
            }
            auto value = emit_argument_for_parameter(argument, function->parameters.front());
            if(not value.valid()) {
                return {};
            }

            auto instruction = emit_value_instruction(ir_opcode::call, function->returns);
            instruction.symbol = constructor;
            instruction.operands = { value };
            return emit(std::move(instruction));
        }

        auto value = emit_expression(argument);
        if(not value.valid()) {
            return {};
        }
        return cast_to(value, info_of(argument).read_type, target_type);
    }

    auto emit_argument_for_parameter(expr_id argument, semantic_type_id parameter) -> ir_value_id
    {
        if(auto const* reference = std::get_if<reference_type>(&module.types.get(parameter))) {
            if(info_of(argument).is_lvalue) {
                return emit_address(argument);
            }

            auto value = emit_expression(argument);
            if(not value.valid()) {
                return {};
            }

            auto address = emit_alloca(reference->pointee, "ref.arg.tmp");
            emit_store(address, value, reference->pointee);
            return address;
        }
        return emit_constructed_value(argument, parameter);
    }

    auto parameter_symbol_for_default(std::size_t context, std::size_t unit, parameter_syntax const& parameter) const -> symbol_id
    {
        auto symbol = semantics.parameter_binding_of(context, unit, parameter.name);
        if(symbol.valid()) {
            return symbol;
        }
        return semantics.parameter_binding_of(unit, parameter.name);
    }

    auto restore_address_bindings(std::vector<address_binding_state> const& bindings) -> void
    {
        for(auto binding = bindings.rbegin(); binding != bindings.rend(); ++binding) {
            if(binding->second) {
                addresses[binding->first] = *binding->second;
            } else {
                addresses.erase(binding->first);
            }
        }
    }

    auto emit_default_argument_for_parameter(symbol_id callee, std::size_t parameter_index, semantic_type_id parameter, std::vector<ir_value_id> const& arguments) -> ir_value_id
    {
        auto const& symbol = semantics.symbols[callee.value];
        auto context = 0uz;
        if(auto instance = semantics.function_instance_of(callee)) {
            context = instance->context_index;
        }
        auto const& function_syntax = units[symbol.unit_index].ast.node(symbol.function);
        if(parameter_index >= function_syntax.parameters.size()) {
            return unsupported_expression("default argument is missing parameter syntax");
        }
        auto const& parameter_syntax = function_syntax.parameters[parameter_index];
        if(not parameter_syntax.default_value) {
            return unsupported_expression("default argument is missing expression");
        }

        auto old_state = save_lowerer_state();
        auto old_bindings = std::vector<address_binding_state>{};

        enter_lowerer_state(symbol.unit_index, context);

        for(auto index = 0uz; index < parameter_index and index < arguments.size() and index < function_syntax.parameters.size(); ++index) {
            auto parameter_symbol = parameter_symbol_for_default(context, unit_index, function_syntax.parameters[index]);
            if(not parameter_symbol.valid()) {
                continue;
            }
            auto found = addresses.find(parameter_symbol);
            old_bindings.emplace_back(
                parameter_symbol,
                found == addresses.end()
                    ? std::optional<ir_value_id>{}
                    : std::optional<ir_value_id>{ found->second }
            );
            auto parameter_type = semantics.symbols[parameter_symbol.value].type;
            auto address = emit_alloca(parameter_type, std::string{ ast_source.slice(function_syntax.parameters[index].name) });
            emit_store(address, arguments[index], parameter_type);
            bind(parameter_symbol, address);
        }

        auto value = emit_argument_for_parameter(*parameter_syntax.default_value, parameter);

        restore_address_bindings(old_bindings);
        restore_lowerer_state(std::move(old_state));

        return value;
    }

    auto explicit_struct_initializer_fields(struct_init_expr_syntax const& node, std::uint32_t struct_index) -> std::set<std::uint64_t>
    {
        auto result = std::set<std::uint64_t>{};
        auto positional_index = 0uz;
        auto const& item = module.structs[struct_index];
        for(auto const& initializer : node.initializers) {
            if(auto const* named = std::get_if<named_field_initializer_syntax>(&initializer)) {
                auto found = std::ranges::find_if(item.fields, [&](semantic_struct_field const& field) {
                    return field.name == ast_source.slice(named->name);
                });
                if(found != item.fields.end()) {
                    result.emplace(static_cast<std::uint64_t>(std::distance(item.fields.begin(), found)));
                }
                continue;
            }
            if(positional_index < item.fields.size()) {
                result.emplace(positional_index);
            }
            ++positional_index;
        }
        return result;
    }

    auto emit_struct_field_default_value(std::uint32_t struct_index, std::uint64_t field_index, semantic_type_id field_type) -> ir_value_id
    {
        auto const& field = module.structs[struct_index].fields[field_index];
        if(not field.default_value) {
            return emit_default_initialized_value(field_type);
        }

        auto old_state = save_lowerer_state();
        enter_lowerer_state(module.structs[struct_index].unit_index, 0uz);
        auto value = ir_value_id{};
        if(is_reference(field_type)) {
            value = emit_address(*field.default_value);
        } else {
            value = emit_constructed_value(*field.default_value, field_type);
        }
        restore_lowerer_state(std::move(old_state));
        return value;
    }

    auto field_default_initialization_of(semantic_type_id type, std::set<semantic_type_id>& visiting) -> field_default_initialization_state
    {
        if(auto found = field_default_initialization_cache.find(type); found != field_default_initialization_cache.end()) {
            return found->second ? field_default_initialization_state::present : field_default_initialization_state::absent;
        }
        if(not visiting.emplace(type).second) {
            return field_default_initialization_state::deferred;
        }

        auto result = field_default_initialization_state::absent;
        if(auto shape = aggregate_shape_of(type)) {
            result = field_default_initialization_of(shape->element, visiting);
        } else if(auto const* instance = std::get_if<struct_type>(&module.types.get(type))) {
            auto struct_index = instance->index;
            auto const& item = module.structs[struct_index];
            if(std::ranges::any_of(item.fields, [](semantic_struct_field const& field) {
                return static_cast<bool>(field.default_value);
            })) {
                result = field_default_initialization_state::present;
            }
            for(auto index = 0uz; index < item.fields.size(); ++index) {
                if(result == field_default_initialization_state::present) {
                    break;
                }
                auto field_access = semantic_field_access {
                    .struct_index = struct_index,
                    .field_index = static_cast<std::uint32_t>(index),
                };
                auto field_type = struct_field_type(type, field_access);
                auto field_result = field_default_initialization_of(field_type, visiting);
                if(field_result == field_default_initialization_state::present) {
                    result = field_default_initialization_state::present;
                    break;
                }
                if(field_result == field_default_initialization_state::deferred) {
                    result = field_default_initialization_state::deferred;
                }
            }
        }

        visiting.erase(type);
        if(result != field_default_initialization_state::deferred) {
            field_default_initialization_cache.emplace(type, result == field_default_initialization_state::present);
        }
        return result;
    }

    auto type_has_field_default_initialization(semantic_type_id type) -> bool
    {
        auto visiting = std::set<semantic_type_id>{};
        auto result = field_default_initialization_of(type, visiting);
        contract_assert(result != field_default_initialization_state::deferred);
        return result == field_default_initialization_state::present;
    }

    auto emit_default_initialized_value(semantic_type_id type) -> ir_value_id
    {
        auto aggregate = emit_default_value(type);
        if(auto shape = aggregate_shape_of(type)) {
            if(not type_has_field_default_initialization(shape->element)) {
                return aggregate;
            }
            for(auto index : std::views::iota(0uz, shape->length)) {
                auto value = emit_default_initialized_value(shape->element);
                if(not value.valid()) {
                    return {};
                }
                aggregate = emit_insert_value(aggregate, value, type, index);
            }
            return aggregate;
        }

        auto const* instance = std::get_if<struct_type>(&module.types.get(type));
        if(instance == nullptr) {
            return aggregate;
        }

        auto struct_index = instance->index;
        auto const& item = module.structs[struct_index];
        for(auto index = 0uz; index < item.fields.size(); ++index) {
            auto field_access = semantic_field_access {
                .struct_index = struct_index,
                .field_index = static_cast<std::uint32_t>(index),
            };
            auto field_type = struct_field_type(type, field_access);
            if(not item.fields[index].default_value and not type_has_field_default_initialization(field_type)) {
                continue;
            }
            auto value = emit_struct_field_default_value(struct_index, index, field_type);
            if(not value.valid()) {
                return {};
            }
            aggregate = emit_insert_value(aggregate, value, type, index);
        }
        return aggregate;
    }

    auto emit_initialize_default_value(ir_value_id address, semantic_type_id type) -> bool
    {
        auto default_value = emit_default_value(type);
        emit_store(address, default_value, type);
        if(auto shape = aggregate_shape_of(type)) {
            if(not type_has_field_default_initialization(shape->element)) {
                return true;
            }
            for(auto index : std::views::iota(0uz, shape->length)) {
                auto index_value = emit_integer_literal(semantic_type_ids::builtin(builtin_type_kind::usize), index);
                auto element_address = emit_element_address(address, index_value, type, shape->element);
                if(not element_address.valid() or not emit_initialize_default_value(element_address, shape->element)) {
                    return false;
                }
            }
            return true;
        }

        auto const* instance = std::get_if<struct_type>(&module.types.get(type));
        if(instance == nullptr) {
            return true;
        }

        auto struct_index = instance->index;
        auto const& item = module.structs[struct_index];
        for(auto index = 0uz; index < item.fields.size(); ++index) {
            auto field_access = semantic_field_access {
                .struct_index = struct_index,
                .field_index = static_cast<std::uint32_t>(index),
            };
            auto field_type = struct_field_type(type, field_access);
            if(not item.fields[index].default_value and not type_has_field_default_initialization(field_type)) {
                continue;
            }
            auto field_address = emit_field_address(address, type, field_type, index);
            auto value = emit_struct_field_default_value(struct_index, index, field_type);
            if(not field_address.valid() or not value.valid()) {
                return false;
            }
            emit_store(field_address, value, field_type);
        }
        return true;
    }

    auto emit_element_address(ir_value_id base, ir_value_id index, semantic_type_id aggregate_type, semantic_type_id element_type) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::element_address, element_type);
        instruction.operands = { base, index };
        instruction.aggregate_type = aggregate_type;
        return emit(std::move(instruction));
    }

    auto emit_cast(ir_value_id value, semantic_type_id type) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::cast, type);
        instruction.operands = { value };
        return emit(std::move(instruction));
    }

    auto emit_aggregate_undef(semantic_type_id type) -> ir_value_id
    {
        return emit(emit_value_instruction(ir_opcode::aggregate_undef, type));
    }

    auto emit_insert_value(ir_value_id aggregate, ir_value_id value, semantic_type_id type, std::uint64_t index) -> ir_value_id
    {
        return emit_insert_value(aggregate, value, type, std::vector<std::uint64_t>{ index });
    }

    auto emit_insert_value(ir_value_id aggregate, ir_value_id value, semantic_type_id type, std::vector<std::uint64_t> indices) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::insert_value, type);
        instruction.operands = { aggregate, value };
        instruction.indices = std::move(indices);
        return emit(std::move(instruction));
    }

    auto emit_extract_value(ir_value_id aggregate, semantic_type_id type, std::uint64_t index) -> ir_value_id
    {
        return emit_extract_value(aggregate, type, std::vector<std::uint64_t>{ index });
    }

    auto emit_extract_value(ir_value_id aggregate, semantic_type_id type, std::vector<std::uint64_t> indices) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::extract_value, type);
        instruction.operands = { aggregate };
        instruction.indices = std::move(indices);
        return emit(std::move(instruction));
    }

    auto emit_str_field(ir_value_id value, std::uint64_t index) -> ir_value_id
    {
        return emit_extract_value(value, str_field_type(index), index);
    }

    auto emit_string_literal(std::string value) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::literal, semantic_type_ids::str);
        instruction.literal = semantic_literal_value {
            .value = std::move(value),
        };
        return emit(std::move(instruction));
    }

    auto emit_panic(ir_value_id message) -> void
    {
        emit_void(ir_instruction {
            .opcode = ir_opcode::panic,
            .type = semantic_type_ids::never,
            .operands = { message },
        });
    }

    auto emit_default_return(semantic_type_id type) -> void
    {
        emit_void(ir_instruction {
            .opcode = ir_opcode::return_,
            .type = type,
        });
    }

    struct cleanup_entry
    {
        cleanup_entry() = default;

        cleanup_entry(symbol_id cleanup_symbol, ir_value_id cleanup_address, semantic_type_id cleanup_type, symbol_id cleanup_destructor) :
            symbol(cleanup_symbol),
            address(cleanup_address),
            type(cleanup_type),
            destructor(cleanup_destructor) {}

        symbol_id symbol{};
        ir_value_id address{};
        semantic_type_id type{};
        symbol_id destructor{};
    };

    auto cleanup_depth() const -> std::size_t
    {
        return cleanups.size();
    }

    auto register_cleanup(symbol_id symbol, ir_value_id address, semantic_type_id type) -> void
    {
        auto destructor = destructor_for(type);
        if(not destructor.valid()) {
            return;
        }
        cleanups.emplace_back(symbol, address, read_type(type), destructor);
    }

    auto emit_cleanups_to(std::size_t depth, symbol_id skipped_symbol = {}) -> void
    {
        for(auto index = cleanups.size(); index > depth; --index) {
            auto const& cleanup = cleanups[index - 1uz];
            if(skipped_symbol.valid() and cleanup.symbol == skipped_symbol) {
                continue;
            }
            emit_void(ir_instruction {
                .opcode = ir_opcode::call,
                .type = semantic_type_ids::unit,
                .operands = { cleanup.address },
                .symbol = cleanup.destructor,
            });
        }
    }

    auto discard_cleanups_to(std::size_t depth) -> void
    {
        cleanups.resize(depth);
    }

    auto emit_statement(stmt_id id) -> bool
    {
        auto const& statement = parsed->ast.node(id);
        return std::visit (
            overloaded {
                [&](block_statement_syntax const& node) {
                    auto depth = cleanup_depth();
                    for(auto child : node.statements) {
                        if(current_block().terminated()) {
                            discard_cleanups_to(depth);
                            return true;
                        }
                        if(not emit_statement(child)) {
                            return false;
                        }
                    }
                    if(not current_block().terminated()) {
                        emit_cleanups_to(depth);
                    }
                    discard_cleanups_to(depth);
                    return true;
                },
                [&](declaration_statement_syntax const& node) {
                    if(not node.binding_names.empty()) {
                        return emit_destructuring_declaration(node);
                    }
                    auto symbol = binding_of(id);
                    if(not symbol.valid()) {
                        return fail("declaration is missing semantic binding");
                    }
                    auto type = semantics.symbols[symbol.value].type;
                    auto address = emit_alloca(type, std::string{ ast_source.slice(node.name) });
                    bind(symbol, address);
                    if(is_reference(type)) {
                        auto initializer = emit_address(node.initializer);
                        if(not initializer.valid()) {
                            return false;
                        }
                        emit_store(address, initializer, type);
                        return true;
                    }
                    if(semantics.direct_initializer_of(current_context_index(), unit_index, id)) {
                        if(not emit_initialize_expression(node.initializer, address, type)) {
                            return current_block().terminated();
                        }
                        register_cleanup(symbol, address, type);
                        return true;
                    }
                    auto initializer = emit_constructed_value(node.initializer, type);
                    if(not initializer.valid()) {
                        return current_block().terminated();
                    }
                    emit_store(address, initializer, type);
                    register_cleanup(symbol, address, type);
                    return true;
                },
                [](type_alias_statement_syntax const&) {
                    return true;
                },
                [&](if_statement_syntax const& node) {
                    return emit_if_statement(node);
                },
                [&](while_statement_syntax const& node) {
                    return emit_while_statement(node);
                },
                [&](do_while_statement_syntax const& node) {
                    return emit_do_while_statement(node);
                },
                [&](return_statement_syntax const& node) {
                    if(node.value) {
                        auto nrvo_candidate = semantics.nrvo_return_of(current_context_index(), unit_index, id);
                        auto value = ir_value_id{};
                        if(is_reference(function->returns)) {
                            value = emit_address(*node.value);
                        } else if(nrvo_candidate.valid()) {
                            value = emit_expression(*node.value);
                        } else {
                            value = emit_constructed_value(*node.value, function->returns);
                        }
                        if(not value.valid()) {
                            return current_block().terminated();
                        }
                        if(not is_reference(function->returns) and nrvo_candidate.valid()) {
                            value = materialize_conversion(*node.value, value);
                        }
                        emit_cleanups_to(0uz, nrvo_candidate);
                        emit_void(ir_instruction {
                            .opcode = ir_opcode::return_,
                            .type = function->returns,
                            .operands = { value },
                        });
                    } else {
                        emit_cleanups_to(0uz);
                        emit_default_return(semantic_type_ids::unit);
                    }
                    return true;
                },
                [&](expression_statement_syntax const& node) {
                    auto value = emit_expression(node.expression);
                    return value.valid() or current_block().terminated();
                },
                [&](break_statement_syntax const& node) {
                    auto target = resolve_loop_jump(node.label, true);
                    if(not target.valid()) {
                        return fail("break has no loop target");
                    }
                    emit_cleanups_to(resolve_loop_cleanup_depth(node.label));
                    emit_void(ir_instruction {
                        .opcode = ir_opcode::branch,
                        .targets = { target },
                    });
                    return true;
                },
                [&](continue_statement_syntax const& node) {
                    auto target = resolve_loop_jump(node.label, false);
                    if(not target.valid()) {
                        return fail("continue has no loop target");
                    }
                    emit_cleanups_to(resolve_loop_cleanup_depth(node.label));
                    emit_void(ir_instruction {
                        .opcode = ir_opcode::branch,
                        .targets = { target },
                    });
                    return true;
                },
                [&](for_statement_syntax const& node) {
                    return emit_for_statement(node, id);
                },
                [&](template_for_statement_syntax const& node) {
                    return emit_template_for_statement(node, id);
                },
            },
            statement
        );
    }

    auto emit_template_for_statement(template_for_statement_syntax const& node, stmt_id id) -> bool
    {
        auto expansions = template_for_expansions_of(id);
        for(auto const& expansion : expansions) {
            if(current_block().terminated()) {
                return true;
            }

            auto depth = cleanup_depth();
            context_stack.emplace_back(expansion.context_index);
            if(expansion.kind == semantic_template_for_expansion_kind::value) {
                auto source_address = address_of(expansion.pack_symbol);
                if(not source_address.valid()) {
                    context_stack.pop_back();
                    return fail("template for value expansion is missing source parameter storage");
                }
                bind(expansion.binding_symbol, source_address);
            }
            if(not emit_statement(node.body)) {
                context_stack.pop_back();
                return false;
            }
            context_stack.pop_back();
            if(not current_block().terminated()) {
                emit_cleanups_to(depth);
            }
            discard_cleanups_to(depth);
        }
        return true;
    }

    auto emit_destructuring_declaration(declaration_statement_syntax const& node) -> bool
    {
        auto initializer_type = info_of(node.initializer).read_type;
        auto const* tuple = std::get_if<tuple_type>(&module.types.get(initializer_type));
        if(tuple == nullptr) {
            return fail("destructuring declaration requires tuple type metadata");
        }

        auto initializer_value = ir_value_id{};
        auto initializer_address = ir_value_id{};
        if(node.is_ref) {
            initializer_address = emit_address(node.initializer);
            if(not initializer_address.valid()) {
                return false;
            }
        } else {
            initializer_value = emit_expression(node.initializer);
            if(not initializer_value.valid()) {
                return current_block().terminated();
            }
        }

        auto count = std::min(tuple->elements.size(), node.binding_names.size());
        for(auto index = 0uz; index < count; ++index) {
            auto binding = node.binding_names[index];
            auto symbol = local_binding_of(binding);
            if(not symbol.valid()) {
                return fail("destructuring binding is missing semantic symbol");
            }

            auto type = semantics.symbols[symbol.value].type;
            auto address = emit_alloca(type, std::string{ ast_source.slice(binding) });
            bind(symbol, address);
            if(node.is_ref) {
                auto element_address = emit_field_address(
                    initializer_address,
                    initializer_type,
                    tuple->elements[index],
                    index
                );
                emit_store(address, element_address, type);
            } else {
                auto element = emit_extract_value(initializer_value, tuple->elements[index], index);
                emit_store(address, element, type);
                register_cleanup(symbol, address, type);
            }
        }
        return true;
    }

    auto emit_if_statement(if_statement_syntax const& node) -> bool
    {
        auto then_block = add_block("if.then");
        auto else_block = node.else_branch ? add_block("if.else") : ir_block_id{};
        auto end_block = add_block("if.end");
        auto condition = emit_expression(node.condition);
        if(not condition.valid()) {
            return false;
        }
        emit_void(ir_instruction {
            .opcode = ir_opcode::cond_branch,
            .operands = { condition },
            .targets = { then_block, node.else_branch ? else_block : end_block },
        });

        current = then_block;
        if(not emit_statement(node.then_branch)) {
            return false;
        }
        if(not current_block().terminated()) {
            emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { end_block } });
        }

        if(node.else_branch) {
            current = else_block;
            if(not emit_statement(*node.else_branch)) {
                return false;
            }
            if(not current_block().terminated()) {
                emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { end_block } });
            }
        }

        current = end_block;
        return true;
    }

    auto emit_while_statement(while_statement_syntax const& node) -> bool
    {
        auto condition_block = add_block("while.cond");
        auto body_block = add_block("while.body");
        auto end_block = add_block("while.end");
        emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { condition_block } });

        current = condition_block;
        auto condition = emit_expression(node.condition);
        if(not condition.valid()) {
            return false;
        }
        emit_void(ir_instruction {
            .opcode = ir_opcode::cond_branch,
            .operands = { condition },
            .targets = { body_block, end_block },
        });

        current = body_block;
        loop_targets.emplace_back(end_block, condition_block, cleanup_depth());
        if(not emit_statement(node.body)) {
            return false;
        }
        loop_targets.pop_back();
        if(not current_block().terminated()) {
            emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { condition_block } });
        }

        current = end_block;
        return true;
    }

    auto emit_do_while_statement(do_while_statement_syntax const& node) -> bool
    {
        auto body_block = add_block("do.body");
        auto condition_block = add_block("do.cond");
        auto end_block = add_block("do.end");
        emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { body_block } });

        current = body_block;
        loop_targets.emplace_back(end_block, condition_block, cleanup_depth());
        if(not emit_statement(node.body)) {
            return false;
        }
        loop_targets.pop_back();
        if(not current_block().terminated()) {
            emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { condition_block } });
        }

        current = condition_block;
        auto condition = emit_expression(node.condition);
        if(not condition.valid()) {
            return false;
        }
        emit_void(ir_instruction {
            .opcode = ir_opcode::cond_branch,
            .operands = { condition },
            .targets = { body_block, end_block },
        });
        current = end_block;
        return true;
    }

    auto emit_for_statement(for_statement_syntax const& node, stmt_id id) -> bool
    {
        auto metadata = for_range_of(id);
        if(metadata.kind == semantic_for_range_kind::iterator_protocol) {
            return emit_protocol_for_statement(node, id, metadata);
        }

        auto range = emit_expression(node.range);
        if(not range.valid()) {
            return false;
        }
        auto range_type = info_of(node.range).read_type;
        auto shape = aggregate_shape_of(range_type);
        if(not shape) {
            return fail("for range lowering requires array type");
        }

        auto symbol = binding_of(id);
        if(not symbol.valid()) {
            return fail("for binding is missing semantic binding");
        }
        auto address = emit_alloca(shape->element, std::string{ ast_source.slice(node.name) });
        bind(symbol, address);

        auto end_block = add_block("for.end");
        if(shape->length == 0uz) {
            emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { end_block } });
            current = end_block;
            return true;
        }

        auto iteration_blocks = std::vector<ir_block_id>{};
        iteration_blocks.reserve(shape->length);
        for(auto index = 0uz; index < shape->length; ++index) {
            iteration_blocks.emplace_back(add_block(std::format("for.iter.{}", index)));
        }

        emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { iteration_blocks.front() } });
        auto label = node.label
            ? std::optional<std::string>{ std::string{ ast_source.slice(*node.label) } }
            : std::nullopt;

        for(auto index = 0uz; index < iteration_blocks.size(); ++index) {
            current = iteration_blocks[index];
            auto next_block = index + 1uz < iteration_blocks.size()
                ? iteration_blocks[index + 1uz]
                : end_block;
            auto value = emit_extract_value(range, shape->element, index);
            emit_store(address, value, shape->element);
            auto depth = cleanup_depth();
            register_cleanup(symbol, address, shape->element);
            loop_targets.emplace_back(end_block, next_block, depth, label);
            if(not emit_statement(node.body)) {
                return false;
            }
            loop_targets.pop_back();
            if(not current_block().terminated()) {
                emit_cleanups_to(depth);
                emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { next_block } });
            }
            discard_cleanups_to(depth);
        }

        current = end_block;
        return true;
    }

    auto emit_protocol_for_statement(for_statement_syntax const& node, stmt_id id, semantic_for_range_info const& metadata) -> bool
    {
        auto const* next_function = callable_type(semantics.symbols[metadata.next_symbol.value].type);
        if(next_function == nullptr or next_function->parameters.empty()) {
            return fail("range next() is missing function type");
        }

        auto iterator_address = ir_value_id{};
        if(metadata.iter_symbol.valid()) {
            iterator_address = emit_alloca(metadata.iterator_type, "for.iter");
            auto const* iter_function = callable_type(semantics.symbols[metadata.iter_symbol.value].type);
            if(iter_function == nullptr or iter_function->parameters.empty()) {
                return fail("range iter() is missing function type");
            }
            auto receiver = ir_value_id{};
            if(is_reference(iter_function->parameters.front())) {
                if(info_of(node.range).is_lvalue) {
                    receiver = emit_address(node.range);
                } else {
                    auto value = emit_expression(node.range);
                    if(not value.valid()) {
                        return false;
                    }
                    receiver = emit_alloca(info_of(node.range).read_type, "for.range.tmp");
                    emit_store(receiver, value, info_of(node.range).read_type);
                }
            } else {
                receiver = emit_expression(node.range);
            }
            if(not receiver.valid()) {
                return false;
            }
            auto instruction = emit_value_instruction(ir_opcode::call, metadata.iterator_type);
            instruction.symbol = metadata.iter_symbol;
            instruction.operands = { receiver };
            auto iterator_value = emit(std::move(instruction));
            emit_store(iterator_address, iterator_value, metadata.iterator_type);
        } else if(is_reference(next_function->parameters.front()) and info_of(node.range).is_lvalue) {
            iterator_address = emit_address(node.range);
            if(not iterator_address.valid()) {
                return false;
            }
        } else {
            iterator_address = emit_alloca(metadata.iterator_type, "for.iter");
            auto iterator_value = emit_expression(node.range);
            if(not iterator_value.valid()) {
                return false;
            }
            emit_store(iterator_address, iterator_value, metadata.iterator_type);
        }

        auto symbol = binding_of(id);
        if(not symbol.valid()) {
            return fail("for binding is missing semantic binding");
        }
        auto value_address = emit_alloca(metadata.element_type, std::string{ ast_source.slice(node.name) });
        bind(symbol, value_address);

        auto condition_block = add_block("for.next");
        auto body_block = add_block("for.body");
        auto end_block = add_block("for.end");
        emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { condition_block } });

        current = condition_block;
        auto next_receiver = ir_value_id{};
        if(is_reference(next_function->parameters.front())) {
            next_receiver = iterator_address;
        } else {
            next_receiver = emit_load(iterator_address, metadata.iterator_type);
        }
        auto next_instruction = emit_value_instruction(ir_opcode::call, next_function->returns);
        next_instruction.symbol = metadata.next_symbol;
        next_instruction.operands = { next_receiver };
        auto next_value = emit(std::move(next_instruction));
        auto tag = emit_extract_value(next_value, semantic_type_ids::i32, 0uz);
        auto some_tag = emit_integer_literal(semantic_type_ids::i32, metadata.some_case_index);
        auto condition = emit_value_instruction(ir_opcode::binary, semantic_type_ids::bool_);
        condition.operator_kind = token_kind::equal_equal;
        condition.operands = { tag, some_tag };
        auto condition_value = emit(std::move(condition));
        emit_void(ir_instruction {
            .opcode = ir_opcode::cond_branch,
            .operands = { condition_value },
            .targets = { body_block, end_block },
        });

        current = body_block;
        auto payload = emit_extract_value(
            next_value,
            metadata.element_type,
            std::vector<std::uint64_t>{ metadata.some_case_index + 1uz, 0uz }
        );
        emit_store(value_address, payload, metadata.element_type);
        auto label = node.label
            ? std::optional<std::string>{ std::string{ ast_source.slice(*node.label) } }
            : std::nullopt;
        auto depth = cleanup_depth();
        register_cleanup(symbol, value_address, metadata.element_type);
        loop_targets.emplace_back(end_block, condition_block, depth, label);
        if(not emit_statement(node.body)) {
            return false;
        }
        loop_targets.pop_back();
        if(not current_block().terminated()) {
            emit_cleanups_to(depth);
            emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { condition_block } });
        }
        discard_cleanups_to(depth);
        current = end_block;
        return true;
    }

    auto emit_expression(expr_id id) -> ir_value_id
    {
        auto const& expression = parsed->ast.node(id);
        auto value = std::visit (
            overloaded {
                [&](name_expr_syntax const& node) {
                    return emit_name_expression(node, id);
                },
                [&](literal_expr_syntax const&) {
                    auto info = info_of(id);
                    auto instruction = emit_value_instruction(ir_opcode::literal, info.read_type);
                    instruction.literal = literal_of(id);
                    return emit(std::move(instruction));
                },
                [&](unary_expr_syntax const& node) {
                    if(selected_operator(id).valid()) {
                        return emit_operator_read(id, std::vector<expr_id>{ node.operand });
                    }
                    return emit_unary_expression(node, id);
                },
                [&](binary_expr_syntax const& node) {
                    if(selected_operator(id).valid()) {
                        return emit_operator_read(id, std::vector<expr_id>{ node.left, node.right });
                    }
                    return emit_binary_expression(node, id);
                },
                [&](assignment_expr_syntax const& node) {
                    if(selected_operator(id).valid()) {
                        return emit_operator_read(id, std::vector<expr_id>{ node.left, node.right });
                    }
                    return emit_assignment_expression(node, id);
                },
                [&](call_expr_syntax const& node) {
                    return emit_call_expression(node, id);
                },
                [&](member_expr_syntax const& node) {
                    auto field = field_access_of(id);
                    if(field.owner_type == semantic_type_ids::str and not info_of(node.object).is_lvalue) {
                        auto object = emit_expression(node.object);
                        if(not object.valid()) {
                            return ir_value_id{};
                        }
                        return emit_str_field(object, field.field_index);
                    }
                    auto address = emit_address(id);
                    if(not address.valid()) {
                        return ir_value_id{};
                    }
                    return emit_load(address, info_of(id).read_type);
                },
                [&](index_expr_syntax const& node) {
                    if(selected_operator(id).valid()) {
                        return emit_operator_read(id, std::vector<expr_id>{ node.object, node.index });
                    }
                    if(as_builtin(info_of(node.object).read_type) == builtin_type_kind::str) {
                        return emit_string_index(node);
                    }
                    auto address = emit_address(id);
                    if(not address.valid()) {
                        return ir_value_id{};
                    }
                    return emit_load(address, info_of(id).read_type);
                },
                [&](associated_name_expr_syntax const&) {
                    auto enum_access = enum_case_of(id);
                    if(enum_access.valid()) {
                        auto const& enum_case = module.enums[enum_access.enum_index].cases[enum_access.case_index];
                        auto instruction = emit_value_instruction(ir_opcode::literal, info_of(id).read_type);
                        instruction.literal.value = enum_case.value;
                        return emit(std::move(instruction));
                    }
                    auto access = variant_case_of(id);
                    if(access.valid()) {
                        return emit_variant_case_value(id, access, {});
                    }
                    return unsupported_expression("associated name cannot be emitted without a call");
                },
                [&](cast_expr_syntax const& node) {
                    auto operand = emit_expression(node.operand);
                    if(not operand.valid()) {
                        return ir_value_id{};
                    }
                    return emit_cast(operand, info_of(id).read_type);
                },
                [&](grouped_expr_syntax const& node) {
                    return emit_expression(node.expression);
                },
                [&](array_literal_expr_syntax const& node) {
                    return emit_array_like_literal(node.elements, info_of(id).read_type);
                },
                [&](tuple_literal_expr_syntax const& node) {
                    return emit_tuple_literal(node.elements, info_of(id).read_type);
                },
                [&](struct_init_expr_syntax const& node) {
                    return emit_struct_initializer(node, id);
                },
                [&](new_expr_syntax const& node) {
                    return emit_new_expression(node, id);
                },
                [&](block_expr_syntax const& node) {
                    return emit_block_expression(node, id);
                },
                [&](match_expr_syntax const& node) {
                    return emit_match_expression(node, id);
                },
                [&](lambda_expr_syntax const& node) {
                    auto lambda = lambda_of(node.function);
                    if(lambda.valid() and lambda.closure_type.valid()) {
                        return emit_lambda_closure(lambda);
                    }
                    auto symbol = resolved_name(id);
                    if(not symbol.valid()) {
                        return unsupported_expression("lambda expression is missing resolved symbol");
                    }
                    auto instruction = emit_value_instruction(ir_opcode::literal, semantics.symbols[symbol.value].type);
                    instruction.symbol = symbol;
                    return emit(std::move(instruction));
                },
            },
            expression
        );
        if(not value.valid()) {
            return value;
        }
        return materialize_conversion(id, value);
    }

    auto emit_initialize_expression(expr_id id, ir_value_id address, semantic_type_id target_type) -> bool
    {
        auto const& expression = parsed->ast.node(stripped_expression(id));
        if(auto const* array = std::get_if<array_literal_expr_syntax>(&expression)) {
            return emit_initialize_array_literal(array->elements, address, target_type);
        }
        if(auto const* tuple = std::get_if<tuple_literal_expr_syntax>(&expression)) {
            return emit_initialize_tuple_literal(tuple->elements, address, target_type);
        }
        if(auto const* initializer = std::get_if<struct_init_expr_syntax>(&expression)) {
            return emit_initialize_struct_initializer(*initializer, stripped_expression(id), address, target_type);
        }

        auto value = emit_constructed_value(id, target_type);
        if(not value.valid()) {
            return false;
        }
        emit_store(address, value, target_type);
        return true;
    }

    auto emit_initialize_array_literal(std::vector<expr_id> const& elements, ir_value_id address, semantic_type_id type) -> bool
    {
        auto shape = aggregate_shape_of(type);
        if(not shape) {
            return fail("array literal direct initialization requires array type metadata");
        }
        for(auto index = 0uz; index < elements.size(); ++index) {
            auto index_value = emit_integer_literal(semantic_type_ids::builtin(builtin_type_kind::usize), index);
            auto element_address = emit_element_address(address, index_value, type, shape->element);
            auto value = emit_constructed_value(elements[index], shape->element);
            if(not element_address.valid() or not value.valid()) {
                return false;
            }
            emit_store(element_address, value, shape->element);
        }
        return true;
    }

    auto emit_initialize_tuple_literal(std::vector<expr_id> const& elements, ir_value_id address, semantic_type_id type) -> bool
    {
        auto const* tuple = std::get_if<tuple_type>(&module.types.get(type));
        if(tuple == nullptr) {
            return fail("tuple literal direct initialization requires tuple type metadata");
        }
        for(auto index = 0uz; index < elements.size(); ++index) {
            auto element_address = emit_field_address(address, type, tuple->elements[index], index);
            auto value = emit_constructed_value(elements[index], tuple->elements[index]);
            if(not element_address.valid() or not value.valid()) {
                return false;
            }
            emit_store(element_address, value, tuple->elements[index]);
        }
        return true;
    }

    auto emit_initialize_struct_initializer(struct_init_expr_syntax const& node, expr_id id, ir_value_id address, semantic_type_id type) -> bool
    {
        auto constructor = resolved_name(id);
        if(constructor.valid() and not semantic_function_body_is_defaulted(semantics.symbols[constructor.value].body_kind)) {
            auto value = emit_expression(id);
            if(not value.valid()) {
                return false;
            }
            emit_store(address, value, type);
            return true;
        }

        auto default_value = emit_default_value(type);
        emit_store(address, default_value, type);
        if(type == semantic_type_ids::str) {
            auto positional_index = 0uz;
            for(auto const& initializer : node.initializers) {
                auto field = std::optional<std::uint64_t>{};
                auto value_id = expr_id{};
                if(auto const* named = std::get_if<named_field_initializer_syntax>(&initializer)) {
                    value_id = named->value;
                    if(ast_source.slice(named->name) == "ptr") {
                        field = 0uz;
                    } else if(ast_source.slice(named->name) == "len") {
                        field = 1uz;
                    }
                } else {
                    value_id = std::get<positional_initializer_syntax>(initializer).value;
                    field = positional_index++;
                }
                if(not field or *field >= 2uz) {
                    continue;
                }
                auto field_address = emit_field_address(address, type, str_field_type(static_cast<std::uint32_t>(*field)), *field);
                auto value = emit_constructed_value(value_id, str_field_type(static_cast<std::uint32_t>(*field)));
                if(not field_address.valid() or not value.valid()) {
                    return false;
                }
                emit_store(field_address, value, str_field_type(static_cast<std::uint32_t>(*field)));
            }
            return true;
        }
        if(auto shape = aggregate_shape_of(type)) {
            auto positional = positional_initializers(node);
            for(auto index = 0uz; index < positional.size(); ++index) {
                auto index_value = emit_integer_literal(semantic_type_ids::builtin(builtin_type_kind::usize), index);
                auto element_address = emit_element_address(address, index_value, type, shape->element);
                auto value = emit_constructed_value(positional[index], shape->element);
                if(not element_address.valid() or not value.valid()) {
                    return false;
                }
                emit_store(element_address, value, shape->element);
            }
            return true;
        }

        auto const* instance = std::get_if<struct_type>(&module.types.get(type));
        if(instance == nullptr) {
            return true;
        }
        auto struct_index = instance->index;
        auto const& item = module.structs[struct_index];
        auto explicit_fields = explicit_struct_initializer_fields(node, struct_index);
        for(auto index = 0uz; index < item.fields.size(); ++index) {
            if(explicit_fields.contains(index)) {
                continue;
            }
            auto field_access = semantic_field_access {
                .struct_index = struct_index,
                .field_index = static_cast<std::uint32_t>(index),
            };
            auto field_type = struct_field_type(type, field_access);
            if(not item.fields[index].default_value and not type_has_field_default_initialization(field_type)) {
                continue;
            }
            auto field_address = emit_field_address(address, type, field_type, index);
            if(not field_address.valid()) {
                return false;
            }
            if(item.fields[index].default_value) {
                auto value = emit_struct_field_default_value(struct_index, index, field_type);
                if(not value.valid()) {
                    return false;
                }
                emit_store(field_address, value, field_type);
            } else if(not emit_initialize_default_value(field_address, field_type)) {
                return false;
            }
        }
        auto positional_index = 0uz;
        for(auto const& initializer : node.initializers) {
            auto field = std::optional<std::uint64_t>{};
            auto value_id = expr_id{};
            if(auto const* named = std::get_if<named_field_initializer_syntax>(&initializer)) {
                value_id = named->value;
                for(auto index = 0uz; index < item.fields.size(); ++index) {
                    if(item.fields[index].name == ast_source.slice(named->name)) {
                        field = index;
                        break;
                    }
                }
            } else {
                value_id = std::get<positional_initializer_syntax>(initializer).value;
                field = positional_index++;
            }
            if(not field) {
                continue;
            }
            auto field_access = semantic_field_access {
                .struct_index = struct_index,
                .field_index = static_cast<std::uint32_t>(*field),
            };
            auto field_type = struct_field_type(type, field_access);
            auto field_address = emit_field_address(address, type, field_type, *field);
            auto value = is_reference(field_type) ? emit_address(value_id) : emit_constructed_value(value_id, field_type);
            if(not field_address.valid() or not value.valid()) {
                return false;
            }
            emit_store(field_address, value, field_type);
        }
        return true;
    }

    auto stripped_expression(expr_id id) const -> expr_id
    {
        auto current = id;
        while(true) {
            auto const& expression = parsed->ast.node(current);
            if(auto const* grouped = std::get_if<grouped_expr_syntax>(&expression)) {
                current = grouped->expression;
                continue;
            }
            return current;
        }
    }

    auto emit_array_like_literal(std::vector<expr_id> const& elements, semantic_type_id type) -> ir_value_id
    {
        auto shape = aggregate_shape_of(type);
        if(not shape) {
            return unsupported_expression("array literal is missing aggregate type");
        }
        auto aggregate = emit_aggregate_undef(type);
        for(auto index = 0uz; index < elements.size(); ++index) {
            auto value = emit_constructed_value(elements[index], shape->element);
            if(not value.valid()) {
                return {};
            }
            aggregate = emit_insert_value(aggregate, value, type, index);
        }
        return aggregate;
    }

    auto emit_lambda_closure(semantic_lambda_info const& lambda) -> ir_value_id
    {
        auto aggregate = emit_aggregate_undef(lambda.closure_type);
        for(auto index = 0uz; index < lambda.captures.size(); ++index) {
            auto const& capture = lambda.captures[index];
            auto value = emit_symbol_read(capture.symbol, capture.type);
            if(not value.valid()) {
                return {};
            }
            aggregate = emit_insert_value(aggregate, value, lambda.closure_type, index);
        }
        return aggregate;
    }

    auto emit_tuple_literal(std::vector<expr_id> const& elements, semantic_type_id type) -> ir_value_id
    {
        auto const* tuple = std::get_if<tuple_type>(&module.types.get(type));
        if(tuple == nullptr) {
            return unsupported_expression("tuple literal is missing tuple type");
        }
        auto aggregate = emit_aggregate_undef(type);
        for(auto index = 0uz; index < elements.size(); ++index) {
            auto value = emit_constructed_value(elements[index], tuple->elements[index]);
            if(not value.valid()) {
                return {};
            }
            aggregate = emit_insert_value(aggregate, value, type, index);
        }
        return aggregate;
    }

    auto emit_struct_initializer(struct_init_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        auto constructor = resolved_name(id);
        auto type = info_of(id).read_type;
        if(constructor.valid() and not semantic_function_body_is_defaulted(semantics.symbols[constructor.value].body_kind)) {
            auto const* function = std::get_if<function_type>(&module.types.get(semantics.symbols[constructor.value].type));
            if(function == nullptr) {
                return unsupported_expression("constructor is missing function type");
            }
            auto arguments = std::vector<ir_value_id>{};
            auto positional = positional_initializers(node);
            for(auto index = 0uz; index < positional.size(); ++index) {
                auto value = emit_argument_for_parameter(positional[index], function->parameters[index]);
                if(not value.valid()) {
                    return {};
                }
                arguments.emplace_back(value);
            }
            auto instruction = emit_value_instruction(ir_opcode::call, type);
            instruction.symbol = constructor;
            instruction.operands = std::move(arguments);
            return emit(std::move(instruction));
        }

        auto aggregate = emit_default_value(type);
        if(type == semantic_type_ids::str) {
            auto positional_index = 0uz;
            for(auto const& initializer : node.initializers) {
                auto field = std::optional<std::uint64_t>{};
                auto value_id = expr_id{};
                if(auto const* named = std::get_if<named_field_initializer_syntax>(&initializer)) {
                    value_id = named->value;
                    if(ast_source.slice(named->name) == "ptr") {
                        field = 0uz;
                    } else if(ast_source.slice(named->name) == "len") {
                        field = 1uz;
                    }
                } else {
                    value_id = std::get<positional_initializer_syntax>(initializer).value;
                    field = positional_index++;
                }
                if(not field or *field >= 2uz) {
                    continue;
                }
                auto value = emit_constructed_value(value_id, str_field_type(*field));
                if(not value.valid()) {
                    return {};
                }
                aggregate = emit_insert_value(aggregate, value, type, *field);
            }
            return aggregate;
        }
        if(not std::holds_alternative<struct_type>(module.types.get(type))) {
            if(auto const* array = std::get_if<array_type>(&module.types.get(type))) {
                auto positional = positional_initializers(node);
                for(auto index = 0uz; index < positional.size(); ++index) {
                    auto value = emit_constructed_value(positional[index], array->element);
                    if(not value.valid()) {
                        return {};
                    }
                    aggregate = emit_insert_value(aggregate, value, type, index);
                }
                return aggregate;
            }
            return aggregate;
        }
        auto const& instance = std::get<struct_type>(module.types.get(type));
        auto struct_index = instance.index;
        auto const& item = module.structs[struct_index];
        auto explicit_fields = explicit_struct_initializer_fields(node, struct_index);
        for(auto index = 0uz; index < item.fields.size(); ++index) {
            if(explicit_fields.contains(index)) {
                continue;
            }
            auto field_access = semantic_field_access {
                .struct_index = struct_index,
                .field_index = static_cast<std::uint32_t>(index),
            };
            auto field_type = struct_field_type(type, field_access);
            if(not item.fields[index].default_value and not type_has_field_default_initialization(field_type)) {
                continue;
            }
            auto value = ir_value_id{};
            if(item.fields[index].default_value) {
                value = emit_struct_field_default_value(struct_index, index, field_type);
            } else {
                value = emit_default_initialized_value(field_type);
            }
            if(not value.valid()) {
                return {};
            }
            aggregate = emit_insert_value(aggregate, value, type, index);
        }
        auto positional_index = 0uz;
        for(auto const& initializer : node.initializers) {
            auto field = std::optional<std::uint64_t>{};
            auto value_id = expr_id{};
            if(auto const* named = std::get_if<named_field_initializer_syntax>(&initializer)) {
                value_id = named->value;
                for(auto index = 0uz; index < item.fields.size(); ++index) {
                    if(item.fields[index].name == ast_source.slice(named->name)) {
                        field = index;
                        break;
                    }
                }
            } else {
                value_id = std::get<positional_initializer_syntax>(initializer).value;
                field = positional_index++;
            }
            if(not field) {
                continue;
            }
            auto field_access = semantic_field_access {
                .struct_index = struct_index,
                .field_index = static_cast<std::uint32_t>(*field),
            };
            auto field_type = struct_field_type(type, field_access);
            auto value = is_reference(field_type) ? emit_address(value_id) : emit_constructed_value(value_id, field_type);
            if(not value.valid()) {
                return {};
            }
            aggregate = emit_insert_value(aggregate, value, type, *field);
        }
        return aggregate;
    }

    auto positional_initializers(struct_init_expr_syntax const& node) -> std::vector<expr_id>
    {
        auto result = std::vector<expr_id>{};
        for(auto const& initializer : node.initializers) {
            if(auto const* positional = std::get_if<positional_initializer_syntax>(&initializer)) {
                result.emplace_back(positional->value);
            }
        }
        return result;
    }

    auto emit_default_value(semantic_type_id type) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::literal, type);
        instruction.literal = semantic_literal_value{};
        return emit(std::move(instruction));
    }

    auto emit_integer_literal(semantic_type_id type, std::uint64_t value) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::literal, type);
        instruction.literal = semantic_literal_value {
            .value = static_cast<std::int64_t>(value),
        };
        return emit(std::move(instruction));
    }

    auto emit_block_expression(block_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        auto depth = cleanup_depth();
        for(auto statement : node.statements) {
            if(current_block().terminated()) {
                discard_cleanups_to(depth);
                return {};
            }
            if(not emit_statement(statement)) {
                return {};
            }
        }
        if(current_block().terminated()) {
            discard_cleanups_to(depth);
            return {};
        }
        auto value = ir_value_id{};
        if(node.tail) {
            value = emit_expression(*node.tail);
            if(not value.valid()) {
                return {};
            }
        } else {
            value = emit_default_value(info_of(id).read_type);
        }
        if(not current_block().terminated()) {
            emit_cleanups_to(depth);
        }
        discard_cleanups_to(depth);
        return value;
    }

    auto emit_variant_case_value(expr_id id, semantic_variant_case_access access, std::vector<expr_id> const& arguments) -> ir_value_id
    {
        auto type = info_of(id).read_type;
        auto aggregate = emit_aggregate_undef(type);
        auto tag = emit_integer_literal(semantic_type_ids::i32, access.case_index);
        aggregate = emit_insert_value(aggregate, tag, type, 0uz);
        auto const& variant_case = module.variants[access.variant_index].cases[access.case_index];
        auto payloads = variant_case_payload_types(type, variant_case);
        auto count = std::min(payloads.size(), arguments.size());
        for(auto index = 0uz; index < count; ++index) {
            auto value = emit_expression(arguments[index]);
            if(not value.valid()) {
                return {};
            }
            aggregate = emit_insert_value(
                aggregate,
                cast_to(value, info_of(arguments[index]).read_type, payloads[index]),
                type,
                std::vector<std::uint64_t>{ access.case_index + 1uz, index }
            );
        }
        return aggregate;
    }

    auto emit_match_expression(match_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        auto matched = emit_expression(node.value);
        if(not matched.valid()) {
            return {};
        }
        auto matched_type = info_of(node.value).read_type;
        auto variant_index = variant_index_of(matched_type);
        if(not variant_index) {
            return unsupported_expression("match expression is missing variant type");
        }

        auto result_type = info_of(id).type;
        auto result_address = is_unit(result_type) ? ir_value_id{} : emit_alloca(result_type, "match.result");
        auto end_block = add_block("match.end");
        auto arm_blocks = std::vector<ir_block_id>{};
        for(auto index = 0uz; index < node.arms.size(); ++index) {
            arm_blocks.emplace_back(add_block(std::format("match.arm.{}", index)));
        }
        auto wildcard_arm = std::optional<std::size_t>{};
        for(auto index = 0uz; index < node.arms.size(); ++index) {
            if(std::holds_alternative<match_wildcard_pattern_syntax>(node.arms[index].pattern)) {
                wildcard_arm = index;
                break;
            }
        }
        auto fallback_block = wildcard_arm ? arm_blocks[*wildcard_arm] : add_block("match.unreachable");

        auto tag = emit_extract_value(matched, semantic_type_ids::i32, 0uz);
        auto next_check = std::vector<ir_block_id>{};
        for(auto index = 0uz; index < node.arms.size(); ++index) {
            next_check.emplace_back(index + 1uz < node.arms.size() ? add_block(std::format("match.check.{}", index + 1uz)) : fallback_block);
        }

        for(auto index = 0uz; index < node.arms.size(); ++index) {
            if(index != 0uz) {
                current = next_check[index - 1uz];
            }
            auto const& arm = node.arms[index];
            if(auto const* wildcard = std::get_if<match_wildcard_pattern_syntax>(&arm.pattern)) {
                static_cast<void>(wildcard);
                emit_void(ir_instruction {
                    .opcode = ir_opcode::branch,
                    .targets = { arm_blocks[index] },
                });
                continue;
            }

            auto const& pattern = std::get<match_case_pattern_syntax>(arm.pattern);
            auto case_index = module.variants[*variant_index].case_indices.find(std::string{ ast_source.identifier(pattern.name) })->second;
            auto case_tag = emit_integer_literal(semantic_type_ids::i32, case_index);
            auto condition = emit_value_instruction(ir_opcode::binary, semantic_type_ids::bool_);
            condition.operator_kind = token_kind::equal_equal;
            condition.operands = { tag, case_tag };
            auto condition_value = emit(std::move(condition));
            emit_void(ir_instruction {
                .opcode = ir_opcode::cond_branch,
                .operands = { condition_value },
                .targets = { arm_blocks[index], next_check[index] },
            });
        }
        if(not wildcard_arm) {
            current = fallback_block;
            emit_panic(emit_string_literal("invalid match tag"));
        }

        auto reaches_end = false;
        for(auto index = 0uz; index < node.arms.size(); ++index) {
            current = arm_blocks[index];
            bind_match_pattern(node.arms[index].pattern, matched, matched_type);
            auto value = emit_expression(node.arms[index].value);
            if(value.valid() and result_address.valid()) {
                emit_store(result_address, value, result_type);
            }
            if(not current_block().terminated()) {
                emit_void(ir_instruction {
                    .opcode = ir_opcode::branch,
                    .targets = { end_block },
                });
                reaches_end = true;
            }
        }

        if(not reaches_end) {
            current = end_block;
            emit_panic(emit_string_literal("entered unreachable match end"));
            current = arm_blocks.back();
            return {};
        }

        current = end_block;
        if(result_address.valid()) {
            return emit_load(result_address, result_type);
        }
        return emit_default_value(result_type);
    }

    auto bind_match_pattern(match_pattern_syntax const& pattern, ir_value_id matched, semantic_type_id matched_type) -> void
    {
        auto const* case_pattern = std::get_if<match_case_pattern_syntax>(&pattern);
        if(case_pattern == nullptr) {
            return;
        }
        auto variant_index = variant_index_of(matched_type);
        if(not variant_index) {
            return;
        }
        auto const& variant = module.variants[*variant_index];
        auto case_index = variant.case_indices.find(std::string{ ast_source.identifier(case_pattern->name) })->second;
        auto payloads = variant_case_payload_types(matched_type, variant.cases[case_index]);
        auto count = std::min(payloads.size(), case_pattern->bindings.size());
        for(auto index = 0uz; index < count; ++index) {
            auto symbol = pattern_binding_of(case_pattern->bindings[index]);
            if(not symbol.valid()) {
                continue;
            }
            auto value = emit_extract_value(
                matched,
                payloads[index],
                std::vector<std::uint64_t>{ case_index + 1uz, index }
            );
            auto address = emit_alloca(payloads[index], std::string{ ast_source.identifier(case_pattern->bindings[index]) });
            emit_store(address, value, payloads[index]);
            bind(symbol, address);
        }
    }

    auto emit_name_expression(name_expr_syntax const&, expr_id id) -> ir_value_id
    {
        auto capture = lambda_capture_of(id);
        if(capture.valid()) {
            auto lambda = lambda_of(capture.function);
            if(capture.field_index >= lambda.captures.size()) {
                return unsupported_expression("lambda capture field is out of bounds");
            }
            auto address = emit_capture_address(capture);
            if(not address.valid()) {
                return {};
            }
            return emit_load(address, lambda.captures[capture.field_index].type);
        }

        auto symbol = resolved_name(id);
        auto field = field_access_of(id);
        if(field.valid()) {
            auto self = self_symbol();
            if(not self.valid()) {
                return unsupported_expression("implicit field access is missing self");
            }
            auto self_address = emit_symbol_address(self);
            if(not self_address.valid()) {
                return {};
            }
            auto self_type = read_type(semantics.symbols[self.value].type);
            auto address = emit_field_value_address(self_address, self_type, field);
            return emit_load(address, info_of(id).read_type);
        }
        if(not symbol.valid()) {
            auto literal = literal_of(id);
            if(not std::holds_alternative<std::monostate>(literal.value)) {
                auto instruction = emit_value_instruction(ir_opcode::literal, info_of(id).read_type);
                instruction.literal = literal;
                return emit(std::move(instruction));
            }
            return unsupported_expression("name expression is missing resolved symbol");
        }
        if(semantics.symbols[symbol.value].kind == symbol_kind::function) {
            auto instruction = emit_value_instruction(ir_opcode::literal, semantics.symbols[symbol.value].type);
            instruction.symbol = symbol;
            return emit(std::move(instruction));
        }
        return emit_symbol_read(symbol, info_of(id).read_type);
    }

    auto emit_builtin_update_expression(unary_expr_syntax const& node, expr_id id, bool address_result) -> ir_value_id
    {
        auto address = emit_address(node.operand);
        if(not address.valid()) {
            return {};
        }
        auto type = info_of(id).read_type;
        auto current_value = emit_load(address, type);
        if(not current_value.valid()) {
            return {};
        }
        auto instruction = emit_value_instruction(ir_opcode::unary, type);
        instruction.operator_kind = node.operator_kind;
        instruction.operands = { current_value };
        auto updated_value = emit(std::move(instruction));
        emit_store(address, updated_value, type);
        if(address_result) {
            return address;
        }
        return node.position == unary_position::postfix ? current_value : updated_value;
    }

    auto emit_unary_expression(unary_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        if(node.operator_kind == token_kind::plus_plus or node.operator_kind == token_kind::minus_minus) {
            return emit_builtin_update_expression(node, id, false);
        }
        if(node.operator_kind == token_kind::amp) {
            return emit_address(node.operand);
        }
        if(node.operator_kind == token_kind::kw_ref or (node.operator_kind == token_kind::kw_const and node.const_ref)) {
            return emit_address(node.operand);
        }
        if(node.operator_kind == token_kind::kw_move or node.operator_kind == token_kind::kw_forward or node.operator_kind == token_kind::kw_const) {
            return emit_expression(node.operand);
        }
        if(node.operator_kind == token_kind::kw_delete) {
            auto builtin = builtin_call_of(id);
            if(not builtin.type.valid() or is_unit(builtin.type)) {
                return emit_default_value(semantic_type_ids::unit);
            }
            auto pointer = emit_expression(node.operand);
            if(not pointer.valid()) {
                return {};
            }
            emit_destroy_object(pointer, builtin.type);
            emit_void(ir_instruction {
                .opcode = ir_opcode::free_raw,
                .type = semantic_type_ids::unit,
                .operands = { pointer },
                .aggregate_type = builtin.type,
            });
            return emit_default_value(semantic_type_ids::unit);
        }
        if(node.operator_kind == token_kind::star) {
            if(selected_operator(id).valid()) {
                return emit_operator_call(id, std::vector<expr_id>{ node.operand });
            }
            auto pointer = emit_expression(node.operand);
            if(not pointer.valid()) {
                return {};
            }
            return emit_load(pointer, info_of(id).read_type);
        }
        auto operand = emit_expression(node.operand);
        if(not operand.valid()) {
            return {};
        }
        auto instruction = emit_value_instruction(ir_opcode::unary, info_of(id).read_type);
        instruction.operator_kind = node.operator_kind;
        instruction.operands = { operand };
        return emit(std::move(instruction));
    }

    auto emit_new_expression(new_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        auto builtin = builtin_call_of(id);
        auto count = emit_integer_literal(semantic_type_ids::builtin(builtin_type_kind::usize), 1uz);
        auto pointer_instruction = emit_value_instruction(ir_opcode::alloc_raw, result_pointer_type(builtin.type));
        pointer_instruction.operands.emplace_back(count);
        pointer_instruction.aggregate_type = builtin.type;
        auto pointer = emit(std::move(pointer_instruction));
        auto value = emit_constructed_value(node.initializer, builtin.type);
        if(not pointer.valid() or not value.valid()) {
            return {};
        }
        emit_store(pointer, value, builtin.type);
        return pointer;
    }

    auto emit_destroy_object(ir_value_id address, semantic_type_id type) -> void
    {
        auto object_type = read_type(type);
        if(auto const* array = std::get_if<array_type>(&module.types.get(object_type))) {
            auto length = array_length_value(array->length).value_or(0uz);
            for(auto index = length; index > 0uz; --index) {
                auto offset = emit_integer_literal(semantic_type_ids::builtin(builtin_type_kind::usize), index - 1uz);
                auto element = emit_element_address(address, offset, object_type, array->element);
                emit_destroy_object(element, array->element);
            }
            return;
        }

        auto destructor = destructor_for(type);
        if(destructor.valid()) {
            emit_void(ir_instruction {
                .opcode = ir_opcode::call,
                .type = semantic_type_ids::unit,
                .operands = { address },
                .symbol = destructor,
            });
        }
    }

    auto emit_binary_expression(binary_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        if(node.operator_kind == token_kind::kw_and or node.operator_kind == token_kind::kw_or) {
            return emit_short_circuit_binary_expression(node, id);
        }

        auto left = emit_expression(node.left);
        auto right = emit_expression(node.right);
        if(not left.valid() or not right.valid()) {
            return {};
        }
        if(auto pointer = pointer_arithmetic_pointee(node, id)) {
            auto instruction = emit_value_instruction(ir_opcode::binary, info_of(id).read_type);
            instruction.operator_kind = node.operator_kind;
            instruction.aggregate_type = *pointer;
            if(pointer_value_pointee(info_of(node.left).read_type)) {
                instruction.operands = { left, right };
            } else {
                instruction.operands = { right, left };
            }
            return emit(std::move(instruction));
        }
        if(auto pointer = pointer_difference_pointee(node, id)) {
            auto instruction = emit_value_instruction(ir_opcode::binary, info_of(id).read_type);
            instruction.operator_kind = node.operator_kind;
            instruction.aggregate_type = *pointer;
            instruction.operands = { left, right };
            return emit(std::move(instruction));
        }
        auto instruction = emit_value_instruction(ir_opcode::binary, info_of(id).read_type);
        instruction.operator_kind = node.operator_kind;
        auto operand_type = binary_operand_type(node, id);
        instruction.operands = {
            cast_to(left, info_of(node.left).read_type, operand_type),
            cast_to(right, info_of(node.right).read_type, operand_type),
        };
        return emit(std::move(instruction));
    }

    auto emit_short_circuit_binary_expression(binary_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        auto result_type = info_of(id).read_type;
        auto result_address = emit_alloca(result_type, node.operator_kind == token_kind::kw_and ? "and.result" : "or.result");
        auto left = emit_expression(node.left);
        if(not left.valid()) {
            return {};
        }

        auto rhs_block = add_block(node.operator_kind == token_kind::kw_and ? "and.rhs" : "or.rhs");
        auto short_block = add_block(node.operator_kind == token_kind::kw_and ? "and.short" : "or.short");
        auto end_block = add_block(node.operator_kind == token_kind::kw_and ? "and.end" : "or.end");
        emit_void(ir_instruction {
            .opcode = ir_opcode::cond_branch,
            .operands = { left },
            .targets = node.operator_kind == token_kind::kw_and
                ? std::vector<ir_block_id>{ rhs_block, short_block }
                : std::vector<ir_block_id>{ short_block, rhs_block },
        });

        current = short_block;
        auto short_value = emit_integer_literal(result_type, node.operator_kind == token_kind::kw_and ? 0uz : 1uz);
        emit_store(result_address, short_value, result_type);
        emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { end_block } });

        current = rhs_block;
        auto right = emit_expression(node.right);
        if(not right.valid()) {
            return {};
        }
        emit_store(result_address, cast_to(right, info_of(node.right).read_type, result_type), result_type);
        if(not current_block().terminated()) {
            emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { end_block } });
        }

        current = end_block;
        return emit_load(result_address, result_type);
    }

    auto pointer_arithmetic_pointee(binary_expr_syntax const& node, expr_id id) -> std::optional<semantic_type_id>
    {
        auto result_type = info_of(id).read_type;
        if(not pointer_value_pointee(result_type)) {
            return std::nullopt;
        }
        if(node.operator_kind != token_kind::plus and node.operator_kind != token_kind::minus) {
            return std::nullopt;
        }
        return pointer_value_pointee(result_type);
    }

    auto pointer_difference_pointee(binary_expr_syntax const& node, expr_id id) -> std::optional<semantic_type_id>
    {
        auto result_type = info_of(id).read_type;
        if(result_type != semantic_type_ids::builtin(builtin_type_kind::isize)) {
            return std::nullopt;
        }
        if(node.operator_kind != token_kind::minus) {
            return std::nullopt;
        }
        auto left = pointer_value_pointee(info_of(node.left).read_type);
        auto right = pointer_value_pointee(info_of(node.right).read_type);
        if(left and right and *left == *right) {
            return *left;
        }
        return std::nullopt;
    }

    auto binary_operand_type(binary_expr_syntax const& node, expr_id id) -> semantic_type_id
    {
        auto result_type = info_of(id).read_type;
        using enum token_kind;
        switch(node.operator_kind) {
            case equal_equal:
            case bang_equal:
            case less:
            case less_equal:
            case spaceship:
            case greater:
            case greater_equal:
                return common_operand_type(
                    info_of(node.left).read_type,
                    info_of(node.right).read_type
                );
            default:
                return result_type;
        }
    }

    auto common_operand_type(semantic_type_id left, semantic_type_id right) -> semantic_type_id
    {
        if(left == right) {
            return left;
        }
        if(pointer_value_pointee(left) and is_nullptr(right)) {
            return left;
        }
        if(is_nullptr(left) and pointer_value_pointee(right)) {
            return right;
        }
        auto left_builtin = as_builtin(left);
        auto right_builtin = as_builtin(right);
        if(not left_builtin or not right_builtin) {
            return left;
        }
        if(is_float(*left_builtin) or is_float(*right_builtin)) {
            auto kind = float_rank(*left_builtin) >= float_rank(*right_builtin) ? *left_builtin : *right_builtin;
            if(not is_float(kind)) {
                kind = builtin_type_kind::f64;
            }
            return semantic_type_ids::builtin(kind);
        }
        if(is_integer(*left_builtin) and is_integer(*right_builtin)) {
            return semantic_type_ids::builtin(
                integer_rank(*left_builtin) >= integer_rank(*right_builtin) ? *left_builtin : *right_builtin
            );
        }
        return left;
    }

    auto pointer_value_pointee(semantic_type_id type) const -> std::optional<semantic_type_id>
    {
        auto const& kind = module.types.get(read_type(type));
        if(auto const* pointer = std::get_if<pointer_type>(&kind)) {
            return pointer->pointee;
        }
        return std::nullopt;
    }

    auto emit_assignment_expression(assignment_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        auto address = emit_address(node.left);
        if(not address.valid()) {
            return {};
        }
        auto type = info_of(id).read_type;

        auto value = ir_value_id{};
        if(node.operator_kind == token_kind::equal) {
            value = emit_expression(node.right);
            if(not value.valid()) {
                return {};
            }
            value = cast_to(value, info_of(node.right).read_type, type);
        } else {
            auto current_value = emit_load(address, type);
            auto right_value = emit_expression(node.right);
            if(not current_value.valid() or not right_value.valid()) {
                return {};
            }
            auto instruction = emit_value_instruction(ir_opcode::binary, type);
            instruction.operator_kind = compound_assignment_operator(node.operator_kind);
            instruction.operands = {
                current_value,
                cast_to(right_value, info_of(node.right).read_type, type),
            };
            value = emit(std::move(instruction));
        }

        emit_store(address, value, type);
        return value;
    }

    auto compound_assignment_operator(token_kind kind) -> token_kind
    {
        using enum token_kind;
        switch(kind) {
            case plus_equal: return plus;
            case minus_equal: return minus;
            case star_equal: return star;
            case slash_equal: return slash;
            case percent_equal: return percent;
            case amp_equal: return amp;
            case pipe_equal: return pipe;
            case caret_equal: return caret;
            case less_less_equal: return less_less;
            case greater_greater_equal: return greater_greater;
            default: std::unreachable();
        }
    }

    auto emit_call_expression(call_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        auto value = emit_call_expression_raw(node, id);
        if(not value.valid()) {
            return {};
        }
        if(info_of(id).is_lvalue) {
            return emit_load(value, info_of(id).read_type);
        }
        return value;
    }

    auto emit_call_expression_raw(call_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        auto builtin = builtin_call_of(id);
        if(builtin.type.valid()) {
            return emit_builtin_call(node, builtin);
        }

        auto const& callee_syntax = parsed->ast.node(node.callee);
        if(auto const* name = std::get_if<name_expr_syntax>(&callee_syntax); name != nullptr and ast_source.identifier(name->name) == "builtin") {
            if(node.arguments.empty()) {
                return unsupported_expression("builtin expression is missing argument");
            }
            return emit_expression(node.arguments.front());
        }

        auto variant_case = variant_case_of(node.callee);
        if(variant_case.valid()) {
            return emit_variant_case_value(id, variant_case, node.arguments);
        }

        if(auto const* member = std::get_if<member_expr_syntax>(&callee_syntax)) {
            return emit_member_call_expression(*member, node, id);
        }

        if(selected_operator(id).valid()) {
            auto arguments = std::vector<expr_id>{ node.callee };
            arguments.insert(arguments.end(), node.arguments.begin(), node.arguments.end());
            return emit_operator_call(id, arguments);
        }

        auto lambda_call = lambda_call_of(id);
        if(lambda_call.valid()) {
            return emit_closure_call_expression(node, id, lambda_call);
        }

        auto closure = semantics.lambda_of_closure(info_of(node.callee).read_type);
        if(closure.valid()) {
            return emit_closure_call_expression(node, id, closure);
        }

        auto callee = resolved_name(node.callee);
        if(not callee.valid()) {
            return unsupported_expression("call callee is missing resolved symbol");
        }
        auto arguments = std::vector<ir_value_id>{};
        auto const* function = callable_type(semantics.symbols[callee.value].type);
        if(function == nullptr) {
            return unsupported_expression("call callee is missing function type");
        }
        auto parameter_offset = 0uz;
        if(
            std::holds_alternative<name_expr_syntax>(callee_syntax)
            and semantics.symbols[callee.value].function_kind == semantic_function_kind::member_function
        ) {
            if(function->parameters.empty()) {
                return unsupported_expression("implicit self call is missing receiver parameter");
            }
            auto self = self_symbol();
            if(not self.valid()) {
                return unsupported_expression("implicit self call is missing self");
            }
            auto receiver = ir_value_id{};
            if(is_reference(function->parameters.front())) {
                receiver = emit_symbol_address(self);
            } else {
                receiver = emit_symbol_read(self, read_type(function->parameters.front()));
            }
            if(not receiver.valid()) {
                return {};
            }
            arguments.emplace_back(receiver);
            parameter_offset = 1uz;
        }
        for(auto index = 0uz; index < node.arguments.size(); ++index) {
            auto argument = node.arguments[index];
            auto value = ir_value_id{};
            auto parameter_index = index + parameter_offset;
            if(parameter_index >= function->parameters.size()) {
                return unsupported_expression("call argument is missing parameter type");
            }
            value = emit_argument_for_parameter(argument, function->parameters[parameter_index]);
            if(not value.valid()) {
                return {};
            }
            arguments.emplace_back(value);
        }
        for(auto parameter_index = node.arguments.size() + parameter_offset; parameter_index < function->parameters.size(); ++parameter_index) {
            auto defaults = semantics.parameter_defaults_of(callee);
            if(defaults == nullptr or parameter_index >= defaults->size() or not (*defaults)[parameter_index]) {
                return unsupported_expression("call argument is missing parameter type");
            }
            auto value = emit_default_argument_for_parameter(callee, parameter_index, function->parameters[parameter_index], arguments);
            if(not value.valid()) {
                return {};
            }
            arguments.emplace_back(value);
        }
        auto instruction = emit_value_instruction(ir_opcode::call, info_of(id).type);
        if(semantics.symbols[callee.value].kind == symbol_kind::function) {
            instruction.symbol = callee;
        } else {
            auto callee_value = emit_expression(node.callee);
            if(not callee_value.valid()) {
                return {};
            }
            arguments.insert(arguments.begin(), callee_value);
            instruction.aggregate_type = read_type(info_of(node.callee).type);
        }
        instruction.operands = std::move(arguments);
        auto result = emit(std::move(instruction));
        return is_never(info_of(id).type) ? ir_value_id{} : result;
    }

    auto emit_closure_call_expression(call_expr_syntax const& node, expr_id id, semantic_lambda_info const& lambda) -> ir_value_id
    {
        auto environment = ir_value_id{};
        auto callee_info = info_of(node.callee);
        if(callee_info.is_lvalue) {
            environment = emit_address(node.callee);
        } else {
            auto value = emit_expression(node.callee);
            if(not value.valid()) {
                return {};
            }
            environment = emit_alloca(lambda.closure_type, "closure.call.tmp");
            emit_store(environment, value, lambda.closure_type);
        }
        if(not environment.valid()) {
            return {};
        }

        auto arguments = std::vector<ir_value_id>{ environment };
        for(auto index = 0uz; index < node.arguments.size(); ++index) {
            auto argument = node.arguments[index];
            auto parameter = lambda.callable.parameters[index];
            auto value = emit_argument_for_parameter(argument, parameter);
            if(not value.valid()) {
                return {};
            }
            arguments.emplace_back(value);
        }

        auto instruction = emit_value_instruction(ir_opcode::call, lambda.callable.returns);
        instruction.symbol = lambda.function_symbol;
        instruction.operands = std::move(arguments);
        auto result = emit(std::move(instruction));
        return is_never(lambda.callable.returns) ? ir_value_id{} : result;
    }

    auto emit_builtin_call(call_expr_syntax const& node, semantic_builtin_call builtin) -> ir_value_id
    {
        using enum semantic_builtin_call_kind;
        switch(builtin.kind) {
            case panic: {
                auto message = emit_expression(node.arguments.front());
                if(not message.valid()) {
                    return {};
                }
                emit_panic(message);
                return {};
            }
            case assert_: {
                if(options.elide_asserts) {
                    return emit_default_value(semantic_type_ids::unit);
                }
                auto condition = emit_expression(node.arguments.front());
                if(not condition.valid()) {
                    return {};
                }
                auto fail_block = add_block("assert.fail");
                auto ok_block = add_block("assert.ok");
                emit_void(ir_instruction {
                    .opcode = ir_opcode::cond_branch,
                    .operands = { condition },
                    .targets = { ok_block, fail_block },
                });

                current = fail_block;
                auto message = ir_value_id{};
                if(node.arguments.size() > 1uz) {
                    message = emit_expression(node.arguments[1uz]);
                } else {
                    message = emit_string_literal("assertion failed");
                }
                if(not message.valid()) {
                    return {};
                }
                emit_panic(message);
                current = ok_block;
                return emit_default_value(semantic_type_ids::unit);
            }
            case unreachable:
                emit_panic(emit_string_literal("entered unreachable code"));
                return {};
            case alloc: {
                auto count = emit_expression(node.arguments.front());
                if(not count.valid()) {
                    return {};
                }
                auto instruction = emit_value_instruction(ir_opcode::alloc_raw, result_pointer_type(builtin.type));
                instruction.operands = { count };
                instruction.aggregate_type = builtin.type;
                return emit(std::move(instruction));
            }
            case free: {
                auto pointer = emit_expression(node.arguments.front());
                if(not pointer.valid()) {
                    return {};
                }
                emit_void(ir_instruction {
                    .opcode = ir_opcode::free_raw,
                    .type = semantic_type_ids::unit,
                    .operands = { pointer },
                    .aggregate_type = builtin.type,
                });
                return emit_default_value(semantic_type_ids::unit);
            }
            case construct_at: {
                auto pointer = emit_expression(node.arguments.front());
                auto value = emit_constructed_value(node.arguments[1uz], builtin.type);
                if(not pointer.valid() or not value.valid()) {
                    return {};
                }
                emit_store(pointer, value, builtin.type);
                return emit_default_value(semantic_type_ids::unit);
            }
            case destroy_at: {
                auto pointer = emit_expression(node.arguments.front());
                if(not pointer.valid()) {
                    return {};
                }
                emit_destroy_object(pointer, builtin.type);
                return emit_default_value(semantic_type_ids::unit);
            }
            case new_object:
            case delete_object:
                return unsupported_expression("new/delete builtin cannot be emitted as a call");
        }
        std::unreachable();
    }

    auto emit_member_call_expression(member_expr_syntax const& member, call_expr_syntax const& node, expr_id id)
        -> ir_value_id
    {
        auto callee = resolved_name(node.callee);
        if(not callee.valid()) {
            return unsupported_expression("member call callee is missing resolved symbol");
        }
        auto const* function = std::get_if<function_type>(&module.types.get(semantics.symbols[callee.value].type));
        if(function == nullptr or function->parameters.empty()) {
            return unsupported_expression("member call callee is missing method type");
        }

        auto arguments = std::vector<ir_value_id>{};
        auto receiver = emit_argument_for_parameter(member.object, function->parameters.front());
        if(not receiver.valid()) {
            return {};
        }
        arguments.emplace_back(receiver);

        for(auto index = 0uz; index < node.arguments.size(); ++index) {
            auto argument = node.arguments[index];
            auto parameter = function->parameters[index + 1uz];
            auto value = emit_argument_for_parameter(argument, parameter);
            if(not value.valid()) {
                return {};
            }
            arguments.emplace_back(value);
        }
        for(auto parameter_index = node.arguments.size() + 1uz; parameter_index < function->parameters.size(); ++parameter_index) {
            auto defaults = semantics.parameter_defaults_of(callee);
            if(defaults == nullptr or parameter_index >= defaults->size() or not (*defaults)[parameter_index]) {
                return unsupported_expression("member call argument is missing parameter type");
            }
            auto value = emit_default_argument_for_parameter(callee, parameter_index, function->parameters[parameter_index], arguments);
            if(not value.valid()) {
                return {};
            }
            arguments.emplace_back(value);
        }

        auto instruction = emit_value_instruction(ir_opcode::call, info_of(id).type);
        instruction.symbol = callee;
        instruction.operands = std::move(arguments);
        auto result = emit(std::move(instruction));
        return is_never(info_of(id).type) ? ir_value_id{} : result;
    }

    auto emit_operator_call(expr_id id, std::vector<expr_id> const& arguments) -> ir_value_id
    {
        auto symbol = selected_operator(id);
        if(not symbol.valid()) {
            return unsupported_expression("operator expression is missing selected symbol");
        }
        auto const* function = std::get_if<function_type>(&module.types.get(semantics.symbols[symbol.value].type));
        if(function == nullptr or function->parameters.size() != arguments.size()) {
            return unsupported_expression("operator symbol is missing function type");
        }

        auto operands = std::vector<ir_value_id>{};
        operands.reserve(arguments.size());
        for(auto index = 0uz; index < arguments.size(); ++index) {
            auto parameter = function->parameters[index];
            auto value = emit_argument_for_parameter(arguments[index], parameter);
            if(not value.valid()) {
                return {};
            }
            operands.emplace_back(value);
        }

        auto instruction = emit_value_instruction(ir_opcode::call, function->returns);
        instruction.symbol = symbol;
        instruction.operands = std::move(operands);
        auto result = emit(std::move(instruction));
        return is_never(function->returns) ? ir_value_id{} : result;
    }

    auto emit_operator_read(expr_id id, std::vector<expr_id> const& arguments) -> ir_value_id
    {
        auto value = emit_operator_call(id, arguments);
        if(not value.valid()) {
            return {};
        }
        if(info_of(id).is_lvalue) {
            return emit_load(value, info_of(id).read_type);
        }
        return value;
    }

    auto emit_address(expr_id id) -> ir_value_id
    {
        auto const& expression = parsed->ast.node(id);
        if(auto const* name = std::get_if<name_expr_syntax>(&expression)) {
            auto capture = lambda_capture_of(id);
            if(capture.valid()) {
                return emit_capture_address(capture);
            }
            auto symbol = resolved_name(id);
            auto field = field_access_of(id);
            if(field.valid()) {
                auto self = self_symbol();
                auto self_address = emit_symbol_address(self);
                if(not self_address.valid()) {
                    return unsupported_expression("implicit field access is missing self address");
                }
                auto self_type = read_type(semantics.symbols[self.value].type);
                return emit_field_value_address(self_address, self_type, field);
            }
            if(not symbol.valid()) {
                return unsupported_expression("address expression is missing resolved symbol");
            }
            static_cast<void>(name);
            return emit_symbol_address(symbol);
        }
        if(auto const* member = std::get_if<member_expr_syntax>(&expression)) {
            auto field = field_access_of(id);
            if(not field.valid()) {
                return unsupported_expression("member expression is missing field access metadata");
            }
            auto base = emit_address(member->object);
            if(not base.valid()) {
                return {};
            }
            auto object_type = info_of(member->object).read_type;
            return emit_field_value_address(base, object_type, field);
        }
        if(auto const* unary = std::get_if<unary_expr_syntax>(&expression); unary and unary->operator_kind == token_kind::star and not selected_operator(id).valid()) {
            return emit_expression(unary->operand);
        }
        if(auto const* grouped = std::get_if<grouped_expr_syntax>(&expression)) {
            return emit_address(grouped->expression);
        }
        if(auto const* index = std::get_if<index_expr_syntax>(&expression)) {
            if(selected_operator(id).valid()) {
                if(not info_of(id).is_lvalue) {
                    return unsupported_expression("value-returning operator [] has no address");
                }
                return emit_operator_call(id, std::vector<expr_id>{ index->object, index->index });
            }
            auto object_info = info_of(index->object);
            auto object_type = object_info.read_type;
            if(auto pointee = pointer_value_pointee(object_type)) {
                auto pointer = emit_expression(index->object);
                auto index_value = emit_expression(index->index);
                if(not pointer.valid() or not index_value.valid()) {
                    return {};
                }
                auto instruction = emit_value_instruction(ir_opcode::binary, result_pointer_type(*pointee));
                instruction.operator_kind = token_kind::plus;
                instruction.operands = { pointer, index_value };
                instruction.aggregate_type = *pointee;
                return emit(std::move(instruction));
            }
            auto object_address = ir_value_id{};
            if(object_info.is_lvalue) {
                object_address = emit_address(index->object);
            } else {
                auto object_value = emit_expression(index->object);
                if(not object_value.valid()) {
                    return {};
                }
                object_address = emit_alloca(object_type, "array.index.tmp");
                emit_store(object_address, object_value, object_type);
            }
            auto index_value = emit_expression(index->index);
            if(not object_address.valid() or not index_value.valid()) {
                return {};
            }
            if(auto const* array = std::get_if<array_type>(&module.types.get(object_type))) {
                auto checked_index = cast_to(index_value, info_of(index->index).read_type, semantic_type_ids::builtin(builtin_type_kind::usize));
                auto length = emit_integer_literal(semantic_type_ids::builtin(builtin_type_kind::usize), array_length_value(array->length).value_or(0uz));
                auto condition = emit_value_instruction(ir_opcode::binary, semantic_type_ids::bool_);
                condition.operator_kind = token_kind::greater_equal;
                condition.operands = std::vector<ir_value_id>{ checked_index, length };
                auto out_of_bounds = emit(std::move(condition));
                auto fail_block = add_block("array.index.fail");
                auto ok_block = add_block("array.index.ok");
                emit_void(ir_instruction {
                    .opcode = ir_opcode::cond_branch,
                    .operands = { out_of_bounds },
                    .targets = { fail_block, ok_block },
                });

                current = fail_block;
                emit_panic(emit_string_literal("array index out of bounds"));
                current = ok_block;
                index_value = checked_index;
            }
            return emit_element_address(
                object_address,
                index_value,
                object_type,
                info_of(id).read_type
            );
        }
        if(auto const* unary = std::get_if<unary_expr_syntax>(&expression)) {
            if(selected_operator(id).valid()) {
                if(not info_of(id).is_lvalue) {
                    return unsupported_expression("value-returning unary operator has no address");
                }
                return emit_operator_call(id, std::vector<expr_id>{ unary->operand });
            }
            if(
                (unary->operator_kind == token_kind::plus_plus or unary->operator_kind == token_kind::minus_minus)
                and unary->position == unary_position::prefix
            ) {
                return emit_builtin_update_expression(*unary, id, true);
            }
            if(unary->operator_kind == token_kind::star) {
                return emit_expression(unary->operand);
            }
            if(
                unary->operator_kind == token_kind::kw_ref
                or unary->operator_kind == token_kind::kw_move
                or unary->operator_kind == token_kind::kw_forward
                or unary->operator_kind == token_kind::kw_const
            ) {
                return emit_address(unary->operand);
            }
        }
        if(auto const* call = std::get_if<call_expr_syntax>(&expression)) {
            if(not info_of(id).is_lvalue) {
                return unsupported_expression("value-returning call has no address");
            }
            return emit_call_expression_raw(*call, id);
        }
        if(std::holds_alternative<match_expr_syntax>(expression) or std::holds_alternative<block_expr_syntax>(expression)) {
            if(not info_of(id).is_lvalue) {
                return unsupported_expression("value-returning expression has no address");
            }
            return emit_expression(id);
        }
        return unsupported_expression("expression has no address");
    }

    auto emit_capture_address(semantic_lambda_capture_access capture) -> ir_value_id
    {
        auto lambda = lambda_of(capture.function);
        if(not lambda.valid() or not lambda.env_symbol.valid()) {
            return unsupported_expression("lambda capture has no closure environment");
        }
        if(capture.field_index >= lambda.captures.size()) {
            return unsupported_expression("lambda capture field is out of bounds");
        }

        auto environment = emit_symbol_address(lambda.env_symbol);
        if(not environment.valid()) {
            return {};
        }
        return emit_field_address(
            environment,
            lambda.closure_type,
            lambda.captures[capture.field_index].type,
            capture.field_index
        );
    }

    auto emit_string_index(index_expr_syntax const& node) -> ir_value_id
    {
        auto text = emit_expression(node.object);
        auto raw_index = emit_expression(node.index);
        if(not text.valid() or not raw_index.valid()) {
            return {};
        }

        auto index = cast_to(raw_index, info_of(node.index).read_type, semantic_type_ids::builtin(builtin_type_kind::usize));
        auto length = emit_str_field(text, 1uz);
        auto condition = emit_value_instruction(ir_opcode::binary, semantic_type_ids::bool_);
        condition.operator_kind = token_kind::greater_equal;
        condition.operands = std::vector<ir_value_id>{ index, length };
        auto out_of_bounds = emit(std::move(condition));
        auto fail_block = add_block("str.index.fail");
        auto ok_block = add_block("str.index.ok");
        emit_void(ir_instruction {
            .opcode = ir_opcode::cond_branch,
            .operands = { out_of_bounds },
            .targets = { fail_block, ok_block },
        });

        current = fail_block;
        emit_panic(emit_string_literal("string index out of bounds"));

        current = ok_block;
        auto pointer = emit_str_field(text, 0uz);
        auto instruction = emit_value_instruction(ir_opcode::binary, result_pointer_type(semantic_type_ids::char_));
        instruction.operator_kind = token_kind::plus;
        instruction.operands = std::vector<ir_value_id>{ pointer, index };
        instruction.aggregate_type = semantic_type_ids::char_;
        auto address = emit(std::move(instruction));
        return emit_load(address, semantic_type_ids::char_);
    }

    auto materialize_conversion(expr_id id, ir_value_id value) -> ir_value_id
    {
        auto target = conversion_of(id);
        if(not target.valid() or is_error(target) or is_inferred(target) or is_unit(target)) {
            return value;
        }
        return emit_cast(value, target);
    }

    auto cast_to(ir_value_id value, semantic_type_id from, semantic_type_id to) -> ir_value_id
    {
        if(from == to) {
            return value;
        }
        return emit_cast(value, to);
    }

    auto constant_integer_index(expr_id id) -> std::optional<std::int64_t>
    {
        auto const& expression = parsed->ast.node(id);
        if(auto const* literal = std::get_if<literal_expr_syntax>(&expression)) {
            auto text = ast_source.slice(literal->full_span);
            if(text.contains('.') or text.contains('e') or text.contains('E')) {
                return std::nullopt;
            }
            auto value = std::int64_t{};
            auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            if(result.ec != std::errc{}) {
                return std::nullopt;
            }
            return value;
        }
        if(auto const* unary = std::get_if<unary_expr_syntax>(&expression); unary and unary->operator_kind == token_kind::minus) {
            auto value = constant_integer_index(unary->operand);
            if(value) {
                return -*value;
            }
        }
        return std::nullopt;
    }

    auto is_reference(semantic_type_id type) const -> bool
    {
        return std::holds_alternative<reference_type>(module.types.get(type));
    }

    auto result_pointer_type(semantic_type_id pointee) -> semantic_type_id
    {
        return module.types.intern(pointer_type{ pointee });
    }

    auto callable_type(semantic_type_id type) const -> function_type const*
    {
        auto const& kind = module.types.get(type);
        if(auto const* callable = std::get_if<function_type>(&kind)) {
            return callable;
        }
        if(auto const* pointer = std::get_if<pointer_type>(&kind)) {
            return std::get_if<function_type>(&module.types.get(pointer->pointee));
        }
        return nullptr;
    }

    auto read_type(semantic_type_id type) const -> semantic_type_id
    {
        auto current = type;
        while(true) {
            auto const& kind = module.types.get(current);
            if(auto const* reference = std::get_if<reference_type>(&kind)) {
                current = reference->pointee;
                continue;
            }
            return current;
        }
    }

    auto struct_index_of(semantic_type_id type) const -> std::optional<std::uint32_t>
    {
        auto const& kind = module.types.get(read_type(type));
        if(auto const* value = std::get_if<struct_type>(&kind)) {
            return value->index;
        }
        return std::nullopt;
    }

    auto struct_field_type(semantic_type_id object_type, semantic_field_access field) -> semantic_type_id
    {
        if(field.owner_type == semantic_type_ids::str) {
            return str_field_type(field.field_index);
        }
        if(auto const* tuple = std::get_if<tuple_type>(&module.types.get(read_type(object_type))); tuple and field.owner_type == read_type(object_type)) {
            return tuple->elements[field.field_index];
        }
        auto const& item = module.structs[field.struct_index];
        auto field_type = item.fields[field.field_index].type;
        auto const* instance = std::get_if<struct_type>(&module.types.get(read_type(object_type)));
        if(instance == nullptr or instance->index != field.struct_index) {
            return field_type;
        }
        return substitute_type(field_type, instance->arguments);
    }

    auto str_field_type(std::uint64_t index) -> semantic_type_id
    {
        if(index == 0uz) {
            return module.types.intern(pointer_type{ semantic_type_ids::char_, true });
        }
        return semantic_type_ids::builtin(builtin_type_kind::usize);
    }

    auto variant_index_of(semantic_type_id type) const -> std::optional<std::uint32_t>
    {
        auto const& kind = module.types.get(read_type(type));
        if(auto const* value = std::get_if<variant_type>(&kind)) {
            return value->index;
        }
        return std::nullopt;
    }

    auto enum_case_of(expr_id id) const -> semantic_enum_case_access
    {
        return semantics.enum_case_of(current_context_index(), unit_index, id);
    }

    auto substitute_type(semantic_type_id type, std::vector<semantic_type_id> const& arguments) -> semantic_type_id
    {
        auto const& kind = module.types.get(type);
        return std::visit (
            overloaded {
                [&](unit_type const&) { return type; },
                [&](error_type const&) { return type; },
                [&](inferred_type const&) { return type; },
                [&](never_type const&) { return type; },
                [&](builtin_type const&) { return type; },
                [&](generic_parameter_type const& value) {
                    if(value.index < arguments.size()) {
                        return arguments[value.index];
                    }
                    return semantic_type_ids::error;
                },
                [&](associated_type_ref const&) { return type; },
                [&](integer_constant_type const&) { return type; },
                [&](generic_integer_parameter_type const& value) {
                    if(value.index < arguments.size()) {
                        return arguments[value.index];
                    }
                    return semantic_type_ids::error;
                },
                [&](array_type const& value) {
                    return module.types.intern(array_type {
                        .element = substitute_type(value.element, arguments),
                        .length = substitute_type(value.length, arguments),
                    });
                },
                [&](tuple_type const& value) {
                    auto elements = (
                        value.elements
                        | std::views::transform([&](auto element) {
                            return substitute_type(element, arguments);
                        }) | std::ranges::to<std::vector<semantic_type_id>>()
                    );
                    return module.types.intern(tuple_type{ std::move(elements) });
                },
                [&](reference_type const& value) {
                    auto pointee = substitute_type(value.pointee, arguments);
                    if(auto const* inner = std::get_if<reference_type>(&module.types.get(pointee))) {
                        return module.types.intern(reference_type {
                            inner->pointee,
                            value.is_const or inner->is_const,
                            value.reference_kind,
                        });
                    }
                    return module.types.intern(reference_type {
                        pointee,
                        value.is_const,
                        value.reference_kind,
                    });
                },
                [&](pointer_type const& value) {
                    return module.types.intern(pointer_type {
                        substitute_type(value.pointee, arguments),
                        value.is_const,
                        value.is_like,
                    });
                },
                [&](function_type const& value) {
                    auto parameters = (
                        value.parameters
                        | std::views::transform([&](auto parameter) {
                            return substitute_type(parameter, arguments);
                        }) | std::ranges::to<std::vector<semantic_type_id>>()
                    );
                    return module.types.intern(function_type {
                        .parameters = std::move(parameters),
                        .returns = substitute_type(value.returns, arguments),
                    });
                },
            [&](struct_type const& value) {
                auto substituted = (
                        value.arguments
                        | std::views::transform([&](auto argument) {
                            return substitute_type(argument, arguments);
                        }) | std::ranges::to<std::vector<semantic_type_id>>()
                    );
                    return module.types.intern(struct_type {
                        .index = value.index,
                    .arguments = std::move(substituted),
                });
            },
            [&](enum_type const&) { return type; },
            [&](opaque_type const&) { return type; },
            [&](variant_type const& value) {
                auto substituted = (
                        value.arguments
                        | std::views::transform([&](auto argument) {
                            return substitute_type(argument, arguments);
                        }) | std::ranges::to<std::vector<semantic_type_id>>()
                    );
                    return module.types.intern(variant_type {
                        .index = value.index,
                        .arguments = std::move(substituted),
                    });
                },
            },
            kind
        );
    }

    auto variant_case_payload_types(semantic_type_id type, semantic_variant_case const& variant_case) -> std::vector<semantic_type_id>
    {
        auto const* variant = std::get_if<variant_type>(&module.types.get(read_type(type)));
        if(variant == nullptr) {
            return {};
        }

        return (
            variant_case.payload_types
            | std::views::transform([&](auto payload) {
                return substitute_type(payload, variant->arguments);
            }) | std::ranges::to<std::vector<semantic_type_id>>()
        );
    }

    auto destructor_for(semantic_type_id type) const -> symbol_id
    {
        auto owner = struct_index_of(type);
        if(not owner) {
            return {};
        }
        auto symbol = module.structs[*owner].destructor;
        if(not symbol.valid()) {
            return {};
        }

        auto const& value = semantics.symbols[symbol.value];
        if(semantics.generic_parameter_count_of(value.unit_index, value.function) == 0uz) {
            return symbol;
        }

        auto const* instance = std::get_if<struct_type>(&module.types.get(read_type(type)));
        if(instance == nullptr) {
            return symbol;
        }
        for(auto const& function_instance : semantics.function_instances) {
            if(
                function_instance.key.unit_index == value.unit_index
                and function_instance.key.function_id_value == value.function.value
                and function_instance.key.type_arguments == instance->arguments
            ) {
                return function_instance.symbol;
            }
        }
        return symbol;
    }

    auto self_symbol() const -> symbol_id
    {
        for(auto const& [symbol, address] : addresses) {
            static_cast<void>(address);
            if(semantics.symbols[symbol.value].name == "self") {
                return symbol;
            }
        }
        return {};
    }

    auto emit_symbol_read(symbol_id symbol, semantic_type_id read_type) -> ir_value_id
    {
        auto address = address_of(symbol);
        if(not address.valid()) {
            return unsupported_expression("name expression has no bound address");
        }
        auto type = semantics.symbols[symbol.value].type;
        if(is_reference(type)) {
            auto referent = emit_load(address, type);
            return emit_load(referent, read_type);
        }
        return emit_load(address, read_type);
    }

    auto emit_symbol_address(symbol_id symbol) -> ir_value_id
    {
        auto address = address_of(symbol);
        if(not address.valid()) {
            return unsupported_expression("name expression has no bound address");
        }
        auto type = semantics.symbols[symbol.value].type;
        if(is_reference(type)) {
            return emit_load(address, type);
        }
        return address;
    }

    struct aggregate_shape
    {
        semantic_type_id element{};
        std::size_t length{};
    };

    auto aggregate_shape_of(semantic_type_id type) -> std::optional<aggregate_shape>
    {
        auto const& kind = module.types.get(type);
        if(auto const* array = std::get_if<array_type>(&kind)) {
            auto length = array_length_value(array->length);
            if(not length) {
                return std::nullopt;
            }
            return aggregate_shape {
                .element = array->element,
                .length = static_cast<std::size_t>(*length),
            };
        }
        return std::nullopt;
    }

    auto array_length_value(semantic_type_id length_type) const -> std::optional<std::uint64_t>
    {
        auto const& kind = module.types.get(length_type);
        auto const* value = std::get_if<integer_constant_type>(&kind);
        if(value == nullptr or value->value < 0) {
            return std::nullopt;
        }
        return static_cast<std::uint64_t>(value->value);
    }

    auto bind(symbol_id symbol, ir_value_id address) -> void
    {
        addresses[symbol] = address;
    }

    auto address_of(symbol_id symbol) const -> ir_value_id
    {
        if(auto found = addresses.find(symbol); found != addresses.end()) {
            return found->second;
        }
        return {};
    }

    auto unsupported_expression(std::string message) -> ir_value_id
    {
        static_cast<void>(fail(std::move(message)));
        return {};
    }

    auto fail(std::string message) -> bool
    {
        if(error.empty()) {
            error = std::move(message);
        }
        return false;
    }

    struct loop_target
    {
        loop_target() = default;

        loop_target(ir_block_id target_break, ir_block_id target_continue, std::size_t target_cleanup_depth, std::optional<std::string> target_label = std::nullopt) :
            break_target(target_break),
            continue_target(target_continue),
            cleanup_depth(target_cleanup_depth),
            label(std::move(target_label)) {}

        ir_block_id break_target{};
        ir_block_id continue_target{};
        std::size_t cleanup_depth{};
        std::optional<std::string> label{};
    };

    auto resolve_loop_jump(std::optional<source_span> label, bool is_break) -> ir_block_id
    {
        if(loop_targets.empty()) {
            return {};
        }
        if(not label) {
            auto const& target = loop_targets.back();
            return is_break ? target.break_target : target.continue_target;
        }

        auto name = std::string{ ast_source.slice(*label) };
        for(auto target = loop_targets.rbegin(); target != loop_targets.rend(); ++target) {
            if(target->label and *target->label == name) {
                return is_break ? target->break_target : target->continue_target;
            }
        }
        return {};
    }

    auto resolve_loop_cleanup_depth(std::optional<source_span> label) -> std::size_t
    {
        if(loop_targets.empty()) {
            return 0uz;
        }
        if(not label) {
            return loop_targets.back().cleanup_depth;
        }

        auto name = std::string{ ast_source.slice(*label) };
        for(auto target = loop_targets.rbegin(); target != loop_targets.rend(); ++target) {
            if(target->label and *target->label == name) {
                return target->cleanup_depth;
            }
        }
        return 0uz;
    }

    ast_source_view ast_source;
    std::span<parse_result const> units;
    parse_result const* parsed{};
    semantic_result const& semantics;
    ir_module& module;
    ir_emit_options options{};
    std::size_t unit_index{};
    function_id function_id_value{};
    std::size_t context_index{};
    std::vector<std::size_t> context_stack{};
    ir_function* function{};
    ir_block_id current{};
    std::uint32_t next_value{};
    std::map<symbol_id, ir_value_id> addresses{};
    std::map<semantic_type_id, bool> field_default_initialization_cache{};
    std::vector<loop_target> loop_targets{};
    std::vector<cleanup_entry> cleanups{};
    std::string error{};
};

export auto emit_ir(source_manager const& sources, std::span<parse_result const> units, semantic_result const& semantics, ir_emit_options options = {}) -> ir_emit_result
{
    auto result = ir_emit_result {
        .accepted = false,
        .module = ir_module{
            .types = semantics.types,
            .structs = semantics.structs,
            .enums = semantics.enums,
            .opaque_aliases = semantics.opaque_aliases,
            .variants = semantics.variants,
        },
    };
    if(not semantics.accepted()) {
        result.error = "IR emission requires accepted semantic results";
        return result;
    }
    for(auto unit_index : std::views::iota(0uz, units.size())) {
        auto const& parsed = units[unit_index];
        if(not parsed.accepted or not parsed.root) {
            result.error = "IR emission requires accepted parser results";
            return result;
        }
        for(auto function_id : parsed.root->functions) {
            auto symbol = semantics.function_symbol_of(unit_index, function_id);
            if(not symbol.valid() or not semantic_function_body_needs_ir_entry(semantics.symbols[symbol.value].body_kind)) {
                continue;
            }
            if(semantics.generic_parameter_count_of(unit_index, function_id) != 0uz) {
                continue;
            }
            auto lowerer = function_lowerer{ sources, units, semantics, result.module, options, unit_index, function_id };
            if(not lowerer.lower()) {
                result.error = std::move(lowerer.error);
                return result;
            }
        }
        for(auto impl_id : parsed.root->impls) {
            auto const& impl = parsed.ast.node(impl_id);
            for(auto function_id : impl.functions) {
                auto symbol = semantics.function_symbol_of(unit_index, function_id);
                if(
                    not symbol.valid()
                    or not semantic_function_body_has_source(semantics.symbols[symbol.value].body_kind)
                    or semantics.generic_parameter_count_of(unit_index, function_id) != 0uz
                ) {
                    continue;
                }
                auto lowerer = function_lowerer{ sources, units, semantics, result.module, options, unit_index, function_id };
                if(not lowerer.lower()) {
                    result.error = std::move(lowerer.error);
                    return result;
                }
            }
        }
        for(auto impl_id : parsed.root->concept_impls) {
            auto const& impl = parsed.ast.node(impl_id);
            for(auto function_id : impl.functions) {
                auto symbol = semantics.function_symbol_of(unit_index, function_id);
                if(
                    not symbol.valid()
                    or not semantic_function_body_has_source(semantics.symbols[symbol.value].body_kind)
                    or semantics.generic_parameter_count_of(unit_index, function_id) != 0uz
                ) {
                    continue;
                }
                auto lowerer = function_lowerer{ sources, units, semantics, result.module, options, unit_index, function_id };
                if(not lowerer.lower()) {
                    result.error = std::move(lowerer.error);
                    return result;
                }
            }
        }
    }

    for(auto const& instance : semantics.function_instances) {
        auto const& parsed = units[instance.key.unit_index];
        if(not semantic_function_body_has_source(semantics.symbols[instance.symbol.value].body_kind)) {
            continue;
        }
        auto lowerer = function_lowerer{
            sources,
            units,
            semantics,
            result.module,
            options,
            instance.key.unit_index,
            function_id{ instance.key.function_id_value },
            instance.context_index
        };
        if(not lowerer.lower()) {
            result.error = std::move(lowerer.error);
            return result;
        }
    }

    result.accepted = true;
    return result;
}

export auto emit_ir(source_manager const& sources, parse_result const& parsed, semantic_result const& semantics, ir_emit_options options = {}) -> ir_emit_result
{
    return emit_ir(sources, std::span<parse_result const>{ &parsed, 1uz }, semantics, options);
}

auto opcode_name(ir_opcode opcode) -> std::string_view
{
    using enum ir_opcode;
    switch(opcode) {
        case alloca_: return "alloca";
        case literal: return "literal";
        case load: return "load";
        case store: return "store";
        case field_address: return "field_address";
        case element_address: return "element_address";
        case unary: return "unary";
        case binary: return "binary";
        case cast: return "cast";
        case aggregate_undef: return "aggregate_undef";
        case insert_value: return "insert_value";
        case extract_value: return "extract_value";
        case alloc_raw: return "alloc_raw";
        case free_raw: return "free_raw";
        case call: return "call";
        case panic: return "panic";
        case branch: return "branch";
        case cond_branch: return "cond_branch";
        case return_: return "return";
    }
    std::unreachable();
}

auto format_value(ir_value_id value) -> std::string
{
    return std::format("%{}", value.value);
}

auto format_block(ir_block_id block) -> std::string
{
    return std::format("^{}", block.value);
}

auto format_literal(semantic_literal_value const& literal) -> std::string
{
    return std::visit (
        overloaded {
            [](std::monostate) { return std::string{"<none>"}; },
            [](bool value) { return value ? std::string{"true"} : std::string{"false"}; },
            [](std::int64_t value) { return std::format("{}", value); },
            [](double value) { return std::format("{}", value); },
            [](char value) { return std::format("'{}'", value); },
            [](std::string const& value) { return std::format("\"{}\"", value); },
        },
        literal.value
    );
}

export auto dump_ir(ir_module const& module) -> std::string
{
    auto output = std::string{};
    for(auto const& function : module.functions) {
        output += std::format("func {}(", function.name);
        for(auto index = 0uz; index < function.parameters.size(); ++index) {
            if(index != 0uz) {
                output += ", ";
            }
            output += std::format("{} {}", format_value(function.parameters[index].value), function.parameters[index].name);
        }
        output += ")\n";
        for(auto block_index = 0uz; block_index < function.blocks.size(); ++block_index) {
            auto const& block = function.blocks[block_index];
            output += std::format("{} {}:\n", format_block(ir_block_id{static_cast<std::uint32_t>(block_index)}), block.name);
            for(auto const& instruction : block.instructions) {
                output += "  ";
                if(instruction.has_result()) {
                    output += format_value(instruction.result);
                    output += " = ";
                }
                output += opcode_name(instruction.opcode);
                if(instruction.opcode == ir_opcode::literal) {
                    output += " ";
                    output += format_literal(instruction.literal);
                }
                for(auto operand : instruction.operands) {
                    output += " ";
                    output += format_value(operand);
                }
                for(auto target : instruction.targets) {
                    output += " ";
                    output += format_block(target);
                }
                for(auto index : instruction.indices) {
                    output += std::format(" #{}", index);
                }
                if(instruction.symbol.valid()) {
                    output += std::format(" @{}", instruction.symbol.value);
                }
                output += "\n";
            }
        }
    }
    return output;
}
