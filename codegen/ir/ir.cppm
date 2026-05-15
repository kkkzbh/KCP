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
    unary,
    binary,
    cast,
    aggregate_undef,
    insert_value,
    extract_value,
    call,
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
    std::vector<ir_function> functions{};
};

export struct ir_emit_result
{
    bool accepted{};
    ir_module module{};
    std::string error{};
};

struct function_lowerer
{
    function_lowerer(
        source_manager const& sources,
        parse_result const& parsed,
        semantic_result const& semantics,
        ir_module& module,
        std::size_t unit_index,
        function_id id
    ) :
        ast_source(sources),
        parsed(parsed),
        semantics(semantics),
        module(module),
        unit_index(unit_index),
        function_id_value(id) {}

    auto lower() -> bool
    {
        auto const& syntax = parsed.ast.function(function_id_value);
        auto signature_id = semantics.signature_of(unit_index, function_id_value);
        auto symbol = semantics.function_symbol_of(unit_index, function_id_value);
        if(not signature_id.valid() or not symbol.valid()) {
            return fail("function is missing semantic signature or symbol");
        }

        module.functions.emplace_back(
            lowered_function_name(symbol),
            lowered_function_linkage(syntax),
            semantics.signatures[signature_id.value].returns,
            symbol
        );
        function = &module.functions.back();
        current = add_block("entry");

        auto const& signature = semantics.signatures[signature_id.value];
        for(auto index = 0uz; index < syntax.parameters.size(); ++index) {
            auto const& parameter = syntax.parameters[index];
            auto parameter_symbol = semantics.parameter_binding_of(unit_index, parameter.name);
            if(not parameter_symbol.valid()) {
                return fail("parameter is missing semantic binding");
            }

            auto value = make_value();
            function->parameters.emplace_back(
                value,
                signature.parameters[index],
                parameter_symbol,
                std::string{ ast_source.slice(parameter.name) }
            );
            auto address = emit_alloca(signature.parameters[index], function->parameters.back().name);
            bind(parameter_symbol, address);
            emit_store(address, value, signature.parameters[index]);
        }

        if(not emit_statement(syntax.body)) {
            return false;
        }
        if(not current_block().terminated()) {
            emit_default_return(signature.returns);
        }
        return true;
    }

    auto lowered_function_name(symbol_id symbol) -> std::string
    {
        auto const& value = semantics.symbols[symbol.value];
        auto module = parsed_module_name();
        if(module.empty() and value.name == "main") {
            return "main";
        }
        if(module.empty()) {
            return value.name;
        }
        return std::format("cp.{}.{}", module, value.name);
    }

    auto lowered_function_linkage(function_syntax const& syntax) -> ir_linkage
    {
        auto name = ast_source.slice(syntax.name);
        if(parsed_module_name().empty() and name == "main") {
            return ir_linkage::external;
        }
        return syntax.exported ? ir_linkage::external : ir_linkage::internal;
    }

    auto parsed_module_name() -> std::string
    {
        if(not parsed.root or not parsed.root->module_header) {
            return {};
        }
        return ast_source.module_name(parsed.root->module_header->name);
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

    auto emit_insert_value(
        ir_value_id aggregate,
        ir_value_id value,
        semantic_type_id type,
        std::uint64_t index
    ) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::insert_value, type);
        instruction.operands = { aggregate, value };
        instruction.indices = { index };
        return emit(std::move(instruction));
    }

    auto emit_extract_value(ir_value_id aggregate, semantic_type_id type, std::uint64_t index) -> ir_value_id
    {
        auto instruction = emit_value_instruction(ir_opcode::extract_value, type);
        instruction.operands = { aggregate };
        instruction.indices = { index };
        return emit(std::move(instruction));
    }

    auto emit_default_return(semantic_type_id type) -> void
    {
        emit_void(ir_instruction {
            .opcode = ir_opcode::return_,
            .type = type,
        });
    }

    auto emit_statement(stmt_id id) -> bool
    {
        auto const& statement = parsed.ast.statement(id);
        return std::visit (
            overloaded {
                [&](block_statement_syntax const& node) {
                    for(auto child : node.statements) {
                        if(current_block().terminated()) {
                            return true;
                        }
                        if(not emit_statement(child)) {
                            return false;
                        }
                    }
                    return true;
                },
                [&](declaration_statement_syntax const& node) {
                    auto symbol = semantics.binding_of(unit_index, id);
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
                    auto initializer = emit_expression(node.initializer);
                    if(not initializer.valid()) {
                        return false;
                    }
                    emit_store(address, materialize_conversion(node.initializer, initializer), type);
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
                        auto value = emit_expression(*node.value);
                        if(not value.valid()) {
                            return false;
                        }
                        emit_void(ir_instruction {
                            .opcode = ir_opcode::return_,
                            .type = semantics.info_of(unit_index, *node.value).read_type,
                            .operands = { materialize_conversion(*node.value, value) },
                        });
                    } else {
                        emit_default_return(semantic_type_ids::unit);
                    }
                    return true;
                },
                [&](expression_statement_syntax const& node) {
                    return emit_expression(node.expression).valid();
                },
                [&](break_statement_syntax const& node) {
                    auto target = resolve_loop_jump(node.label, true);
                    if(not target.valid()) {
                        return fail("break has no loop target");
                    }
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
                    emit_void(ir_instruction {
                        .opcode = ir_opcode::branch,
                        .targets = { target },
                    });
                    return true;
                },
                [&](for_statement_syntax const& node) {
                    return emit_for_statement(node, id);
                },
            },
            statement
        );
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
        loop_targets.emplace_back(end_block, condition_block);
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
        loop_targets.emplace_back(end_block, condition_block);
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
        auto range = emit_expression(node.range);
        if(not range.valid()) {
            return false;
        }
        auto range_type = semantics.info_of(unit_index, node.range).read_type;
        auto shape = aggregate_shape_of(range_type);
        if(not shape) {
            return fail("for range lowering requires array or sequence type");
        }

        auto symbol = semantics.binding_of(unit_index, id);
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
            loop_targets.emplace_back(end_block, next_block, label);
            if(not emit_statement(node.body)) {
                return false;
            }
            loop_targets.pop_back();
            if(not current_block().terminated()) {
                emit_void(ir_instruction{ .opcode = ir_opcode::branch, .targets = { next_block } });
            }
        }

        current = end_block;
        return true;
    }

    auto emit_expression(expr_id id) -> ir_value_id
    {
        auto const& expression = parsed.ast.expression(id);
        auto value = std::visit (
            overloaded {
                [&](name_expr_syntax const& node) {
                    return emit_name_expression(node, id);
                },
                [&](literal_expr_syntax const&) {
                    auto info = semantics.info_of(unit_index, id);
                    auto instruction = emit_value_instruction(ir_opcode::literal, info.read_type);
                    instruction.literal = semantics.literal_of(unit_index, id);
                    return emit(std::move(instruction));
                },
                [&](unary_expr_syntax const& node) {
                    return emit_unary_expression(node, id);
                },
                [&](binary_expr_syntax const& node) {
                    return emit_binary_expression(node, id);
                },
                [&](assignment_expr_syntax const& node) {
                    return emit_assignment_expression(node, id);
                },
                [&](call_expr_syntax const& node) {
                    return emit_call_expression(node, id);
                },
                [&](cast_expr_syntax const& node) {
                    auto operand = emit_expression(node.operand);
                    if(not operand.valid()) {
                        return ir_value_id{};
                    }
                    return emit_cast(operand, semantics.info_of(unit_index, id).read_type);
                },
                [&](grouped_expr_syntax const& node) {
                    return emit_expression(node.expression);
                },
                [&](array_literal_expr_syntax const& node) {
                    return emit_array_like_literal(node.elements, semantics.info_of(unit_index, id).read_type);
                },
                [&](sequence_literal_expr_syntax const& node) {
                    return emit_array_like_literal(node.elements, semantics.info_of(unit_index, id).read_type);
                },
                [&](tuple_literal_expr_syntax const& node) {
                    return emit_tuple_literal(node.elements, semantics.info_of(unit_index, id).read_type);
                },
            },
            expression
        );
        if(not value.valid()) {
            return value;
        }
        return materialize_conversion(id, value);
    }

    auto emit_array_like_literal(std::vector<expr_id> const& elements, semantic_type_id type) -> ir_value_id
    {
        auto shape = aggregate_shape_of(type);
        if(not shape) {
            return unsupported_expression("array or sequence literal is missing aggregate type");
        }
        auto aggregate = emit_aggregate_undef(type);
        for(auto index = 0uz; index < elements.size(); ++index) {
            auto value = emit_expression(elements[index]);
            if(not value.valid()) {
                return {};
            }
            aggregate = emit_insert_value(aggregate, value, type, index);
        }
        return aggregate;
    }

    auto emit_tuple_literal(std::vector<expr_id> const& elements, semantic_type_id type) -> ir_value_id
    {
        if(not std::holds_alternative<tuple_type>(module.types.get(type))) {
            return unsupported_expression("tuple literal is missing tuple type");
        }
        auto aggregate = emit_aggregate_undef(type);
        for(auto index = 0uz; index < elements.size(); ++index) {
            auto value = emit_expression(elements[index]);
            if(not value.valid()) {
                return {};
            }
            aggregate = emit_insert_value(aggregate, value, type, index);
        }
        return aggregate;
    }

    auto emit_name_expression(name_expr_syntax const&, expr_id id) -> ir_value_id
    {
        auto symbol = semantics.resolved_name(unit_index, id);
        if(not symbol.valid()) {
            return unsupported_expression("name expression is missing resolved symbol");
        }
        if(semantics.symbols[symbol.value].kind == symbol_kind::function) {
            auto instruction = emit_value_instruction(ir_opcode::literal, semantics.symbols[symbol.value].type);
            instruction.symbol = symbol;
            return emit(std::move(instruction));
        }
        return emit_symbol_read(symbol, semantics.info_of(unit_index, id).read_type);
    }

    auto emit_unary_expression(unary_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        if(node.operator_kind == token_kind::amp) {
            return emit_address(node.operand);
        }
        if(node.operator_kind == token_kind::star) {
            auto pointer = emit_expression(node.operand);
            if(not pointer.valid()) {
                return {};
            }
            return emit_load(pointer, semantics.info_of(unit_index, id).read_type);
        }
        auto operand = emit_expression(node.operand);
        if(not operand.valid()) {
            return {};
        }
        auto instruction = emit_value_instruction(ir_opcode::unary, semantics.info_of(unit_index, id).read_type);
        instruction.operator_kind = node.operator_kind;
        instruction.operands = { operand };
        return emit(std::move(instruction));
    }

    auto emit_binary_expression(binary_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        auto left = emit_expression(node.left);
        auto right = emit_expression(node.right);
        if(not left.valid() or not right.valid()) {
            return {};
        }
        auto instruction = emit_value_instruction(ir_opcode::binary, semantics.info_of(unit_index, id).read_type);
        instruction.operator_kind = node.operator_kind;
        auto operand_type = binary_operand_type(node, id);
        instruction.operands = {
            cast_to(left, semantics.info_of(unit_index, node.left).read_type, operand_type),
            cast_to(right, semantics.info_of(unit_index, node.right).read_type, operand_type),
        };
        return emit(std::move(instruction));
    }

    auto binary_operand_type(binary_expr_syntax const& node, expr_id id) -> semantic_type_id
    {
        auto result_type = semantics.info_of(unit_index, id).read_type;
        using enum token_kind;
        switch(node.operator_kind) {
            case equal_equal:
            case bang_equal:
            case less:
            case less_equal:
            case greater:
            case greater_equal:
                return common_operand_type(
                    semantics.info_of(unit_index, node.left).read_type,
                    semantics.info_of(unit_index, node.right).read_type
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

    auto emit_assignment_expression(assignment_expr_syntax const& node, expr_id id) -> ir_value_id
    {
        auto address = emit_address(node.left);
        if(not address.valid()) {
            return {};
        }
        auto type = semantics.info_of(unit_index, id).read_type;

        auto value = ir_value_id{};
        if(node.operator_kind == token_kind::equal) {
            value = emit_expression(node.right);
            if(not value.valid()) {
                return {};
            }
            value = cast_to(value, semantics.info_of(unit_index, node.right).read_type, type);
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
                cast_to(right_value, semantics.info_of(unit_index, node.right).read_type, type),
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
        auto callee = semantics.resolved_name(unit_index, node.callee);
        if(not callee.valid()) {
            if(node.arguments.size() != 1uz) {
                return unsupported_expression("call callee is missing resolved symbol");
            }
            auto value = emit_expression(node.arguments.front());
            if(not value.valid()) {
                return {};
            }
            return emit_cast(value, semantics.info_of(unit_index, id).read_type);
        }
        auto arguments = std::vector<ir_value_id>{};
        auto const* function = std::get_if<function_type>(&module.types.get(semantics.symbols[callee.value].type));
        if(function == nullptr) {
            return unsupported_expression("call callee is missing function type");
        }
        for(auto index = 0uz; index < node.arguments.size(); ++index) {
            auto argument = node.arguments[index];
            auto value = ir_value_id{};
            if(is_reference(function->parameters[index])) {
                value = emit_address(argument);
            } else {
                value = emit_expression(argument);
            }
            if(not value.valid()) {
                return {};
            }
            arguments.emplace_back(value);
        }
        auto instruction = emit_value_instruction(ir_opcode::call, semantics.info_of(unit_index, id).read_type);
        instruction.symbol = callee;
        instruction.operands = std::move(arguments);
        return emit(std::move(instruction));
    }

    auto emit_address(expr_id id) -> ir_value_id
    {
        auto const& expression = parsed.ast.expression(id);
        if(auto const* name = std::get_if<name_expr_syntax>(&expression)) {
            auto symbol = semantics.resolved_name(unit_index, id);
            if(not symbol.valid()) {
                return unsupported_expression("address expression is missing resolved symbol");
            }
            static_cast<void>(name);
            return emit_symbol_address(symbol);
        }
        if(auto const* unary = std::get_if<unary_expr_syntax>(&expression); unary and unary->operator_kind == token_kind::star) {
            return emit_expression(unary->operand);
        }
        return unsupported_expression("expression has no address");
    }

    auto materialize_conversion(expr_id id, ir_value_id value) -> ir_value_id
    {
        auto target = semantics.conversion_of(unit_index, id);
        if(not target.valid()) {
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

    auto is_reference(semantic_type_id type) const -> bool
    {
        return std::holds_alternative<reference_type>(module.types.get(type));
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
            return aggregate_shape {
                .element = array->element,
                .length = static_cast<std::size_t>(array->length),
            };
        }
        if(auto const* sequence = std::get_if<sequence_type>(&kind)) {
            return aggregate_shape {
                .element = sequence->element,
                .length = static_cast<std::size_t>(sequence->length),
            };
        }
        return std::nullopt;
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

        loop_target(
            ir_block_id target_break,
            ir_block_id target_continue,
            std::optional<std::string> target_label = std::nullopt
        ) :
            break_target(target_break),
            continue_target(target_continue),
            label(std::move(target_label)) {}

        ir_block_id break_target{};
        ir_block_id continue_target{};
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

    ast_source_view ast_source;
    parse_result const& parsed;
    semantic_result const& semantics;
    ir_module& module;
    std::size_t unit_index{};
    function_id function_id_value{};
    ir_function* function{};
    ir_block_id current{};
    std::uint32_t next_value{};
    std::map<symbol_id, ir_value_id> addresses{};
    std::vector<loop_target> loop_targets{};
    std::string error{};
};

export auto emit_ir(
    source_manager const& sources,
    std::span<parse_result const> units,
    semantic_result const& semantics
) -> ir_emit_result
{
    auto result = ir_emit_result {
        .accepted = false,
        .module = ir_module{ .types = semantics.types },
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
            auto lowerer = function_lowerer{ sources, parsed, semantics, result.module, unit_index, function_id };
            if(not lowerer.lower()) {
                result.error = std::move(lowerer.error);
                return result;
            }
        }
    }

    result.accepted = true;
    return result;
}

export auto emit_ir(
    source_manager const& sources,
    parse_result const& parsed,
    semantic_result const& semantics
) -> ir_emit_result
{
    return emit_ir(sources, std::span<parse_result const>{ &parsed, 1uz }, semantics);
}

auto opcode_name(ir_opcode opcode) -> std::string_view
{
    using enum ir_opcode;
    switch(opcode) {
        case alloca_: return "alloca";
        case literal: return "literal";
        case load: return "load";
        case store: return "store";
        case unary: return "unary";
        case binary: return "binary";
        case cast: return "cast";
        case aggregate_undef: return "aggregate_undef";
        case insert_value: return "insert_value";
        case extract_value: return "extract_value";
        case call: return "call";
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
