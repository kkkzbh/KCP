module;

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

export module codegen.llvm;

import std;
import lexer.token;
import semantic;
import codegen.ir;

template<class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};

export struct llvm_emit_result
{
    bool verified{};
    std::string ir{};
    std::string error{};
};

struct llvm_type_lowerer
{
    auto lower(semantic_type_id id) -> llvm::Type*
    {
        auto const& kind = module.types.get(id);
        return std::visit(overloaded {
            [&](unit_type const&) -> llvm::Type* {
                return llvm::Type::getVoidTy(context);
            },
            [&](error_type const&) -> llvm::Type* {
                return llvm::Type::getVoidTy(context);
            },
            [&](inferred_type const&) -> llvm::Type* {
                return llvm::Type::getVoidTy(context);
            },
            [&](builtin_type const& type) -> llvm::Type* {
                return lower_builtin(type.kind);
            },
            [&](array_type const& type) -> llvm::Type* {
                return llvm::ArrayType::get(lower(type.element), type.length);
            },
            [&](tuple_type const& type) -> llvm::Type* {
                auto elements = std::vector<llvm::Type*>{};
                for(auto element : type.elements) {
                    elements.emplace_back(lower(element));
                }
                return llvm::StructType::get(context, elements);
            },
            [&](reference_type const&) -> llvm::Type* {
                return llvm::PointerType::get(context, 0);
            },
            [&](pointer_type const&) -> llvm::Type* {
                return llvm::PointerType::get(context, 0);
            },
            [&](function_type const& type) -> llvm::Type* {
                static_cast<void>(type);
                return llvm::PointerType::get(context, 0);
            },
            [&](generic_parameter_type const&) -> llvm::Type* {
                return llvm::Type::getVoidTy(context);
            },
            [&](struct_type const& type) -> llvm::Type* {
                auto elements = std::vector<llvm::Type*>{};
                for(auto const& field : module.structs[type.index].fields) {
                    elements.emplace_back(lower_substituted(field.type, type.arguments));
                }
                return llvm::StructType::get(context, elements);
            },
            [&](variant_type const& type) -> llvm::Type* {
                auto elements = std::vector<llvm::Type*>{ llvm::Type::getInt32Ty(context) };
                auto const& variant = module.variants[type.index];
                for(auto const& variant_case : variant.cases) {
                    auto payloads = std::vector<llvm::Type*>{};
                    for(auto payload : variant_case.payload_types) {
                        payloads.emplace_back(lower_substituted(payload, type.arguments));
                    }
                    elements.emplace_back(llvm::StructType::get(context, payloads));
                }
                return llvm::StructType::get(context, elements);
            },
        }, kind);
    }

    auto lower_substituted(
        semantic_type_id id,
        std::vector<semantic_type_id> const& arguments
    ) -> llvm::Type*
    {
        auto const& kind = module.types.get(id);
        return std::visit(overloaded {
            [&](unit_type const&) -> llvm::Type* { return lower(id); },
            [&](error_type const&) -> llvm::Type* { return lower(id); },
            [&](inferred_type const&) -> llvm::Type* { return lower(id); },
            [&](builtin_type const&) -> llvm::Type* { return lower(id); },
            [&](generic_parameter_type const& parameter) -> llvm::Type* {
                if(parameter.index < arguments.size()) {
                    return lower(arguments[parameter.index]);
                }
                return llvm::Type::getVoidTy(context);
            },
            [&](array_type const& type) -> llvm::Type* {
                return llvm::ArrayType::get(lower_substituted(type.element, arguments), type.length);
            },
            [&](tuple_type const& type) -> llvm::Type* {
                auto elements = std::vector<llvm::Type*>{};
                for(auto element : type.elements) {
                    elements.emplace_back(lower_substituted(element, arguments));
                }
                return llvm::StructType::get(context, elements);
            },
            [&](reference_type const&) -> llvm::Type* { return lower(id); },
            [&](pointer_type const&) -> llvm::Type* { return lower(id); },
            [&](function_type const&) -> llvm::Type* { return lower(id); },
            [&](struct_type const& type) -> llvm::Type* {
                auto elements = std::vector<llvm::Type*>{};
                for(auto const& field : module.structs[type.index].fields) {
                    elements.emplace_back(lower_substituted(field.type, type.arguments));
                }
                return llvm::StructType::get(context, elements);
            },
            [&](variant_type const& type) -> llvm::Type* {
                auto elements = std::vector<llvm::Type*>{ llvm::Type::getInt32Ty(context) };
                auto const& variant = module.variants[type.index];
                for(auto const& variant_case : variant.cases) {
                    auto payloads = std::vector<llvm::Type*>{};
                    for(auto payload : variant_case.payload_types) {
                        payloads.emplace_back(lower_substituted(payload, type.arguments));
                    }
                    elements.emplace_back(llvm::StructType::get(context, payloads));
                }
                return llvm::StructType::get(context, elements);
            },
        }, kind);
    }

    auto lower_builtin(builtin_type_kind kind) -> llvm::Type*
    {
        using enum builtin_type_kind;
        switch(kind) {
            case bool_:
            case i8:
            case u8:
            case char_:
                return llvm::Type::getInt8Ty(context);
            case i16:
            case u16:
                return llvm::Type::getInt16Ty(context);
            case i32:
            case u32:
                return llvm::Type::getInt32Ty(context);
            case i64:
            case u64:
            case isize:
            case usize:
                return llvm::Type::getInt64Ty(context);
            case f32:
                return llvm::Type::getFloatTy(context);
            case f64:
                return llvm::Type::getDoubleTy(context);
            case str:
                return llvm::PointerType::get(context, 0);
        }
        std::unreachable();
    }

    auto lower_function_signature(semantic_type_id id) -> llvm::FunctionType*
    {
        auto const* callable = callable_type(id);
        if(callable == nullptr) {
            return llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
        }

        auto parameters = std::vector<llvm::Type*>{};
        for(auto parameter : callable->parameters) {
            parameters.emplace_back(lower(parameter));
        }
        return llvm::FunctionType::get(lower(callable->returns), parameters, false);
    }

    auto callable_type(semantic_type_id id) const -> function_type const*
    {
        auto const& kind = module.types.get(id);
        if(auto const* callable = std::get_if<function_type>(&kind)) {
            return callable;
        }
        if(auto const* pointer = std::get_if<pointer_type>(&kind)) {
            return std::get_if<function_type>(&module.types.get(pointer->pointee));
        }
        return nullptr;
    }

    llvm::LLVMContext& context;
    ir_module const& module;
};

struct llvm_module_lowerer
{
    explicit llvm_module_lowerer(ir_module const& ir) :
        context(),
        llvm_module("cp_module", context),
        ir(ir),
        types(context, ir),
        builder(context) {}

    auto lower() -> llvm_emit_result
    {
        declare_functions();
        for(auto const& function : ir.functions) {
            lower_function(function);
        }

        auto error = std::string{};
        auto error_stream = llvm::raw_string_ostream{ error };
        auto broken = llvm::verifyModule(llvm_module, &error_stream);
        auto output = std::string{};
        auto output_stream = llvm::raw_string_ostream{ output };
        llvm_module.print(output_stream, nullptr);
        return llvm_emit_result {
            .verified = not broken,
            .ir = output_stream.str(),
            .error = error_stream.str(),
        };
    }

    auto declare_functions() -> void
    {
        for(auto const& function : ir.functions) {
            auto parameters = std::vector<llvm::Type*>{};
            for(auto const& parameter : function.parameters) {
                parameters.emplace_back(types.lower(parameter.type));
            }

            auto function_type = llvm::FunctionType::get(types.lower(function.returns), parameters, false);
            auto linkage = function.linkage == ir_linkage::external
                ? llvm::Function::ExternalLinkage
                : llvm::Function::InternalLinkage;
            auto llvm_function = llvm::Function::Create(
                function_type,
                linkage,
                function.name,
                llvm_module
            );
            functions.emplace(function.symbol, llvm_function);
        }
    }

    auto lower_function(ir_function const& function) -> void
    {
        values.clear();
        blocks.clear();
        auto llvm_function = functions[function.symbol];
        for(auto const& block : function.blocks) {
            blocks.emplace_back(llvm::BasicBlock::Create(context, block.name, llvm_function));
        }

        auto argument = llvm_function->arg_begin();
        for(auto const& parameter : function.parameters) {
            argument->setName(parameter.name);
            values.emplace(parameter.value, &*argument);
            ++argument;
        }

        for(auto index = 0uz; index < function.blocks.size(); ++index) {
            builder.SetInsertPoint(blocks[index]);
            for(auto const& instruction : function.blocks[index].instructions) {
                lower_instruction(instruction);
            }
            if(not blocks[index]->getTerminator()) {
                emit_default_return(function.returns);
            }
        }
    }

    auto lower_instruction(ir_instruction const& instruction) -> void
    {
        using enum ir_opcode;
        switch(instruction.opcode) {
            case alloca_:
                bind(instruction.result, builder.CreateAlloca(types.lower(instruction.type), nullptr, instruction.name));
                break;
            case literal:
                bind(instruction.result, lower_literal(instruction.type, instruction.literal, instruction.symbol));
                break;
            case load:
                bind(
                    instruction.result,
                    builder.CreateLoad(types.lower(instruction.type), value(instruction.operands.front()))
                );
                break;
            case store:
                builder.CreateStore(value(instruction.operands[1]), value(instruction.operands[0]));
                break;
            case field_address:
                bind(
                    instruction.result,
                    builder.CreateStructGEP(
                        types.lower(instruction.aggregate_type),
                        value(instruction.operands.front()),
                        static_cast<unsigned>(instruction.indices.front())
                    )
                );
                break;
            case element_address:
                bind(
                    instruction.result,
                    builder.CreateGEP(
                        types.lower(instruction.aggregate_type),
                        value(instruction.operands[0]),
                        { builder.getInt32(0), value(instruction.operands[1]) }
                    )
                );
                break;
            case unary:
                bind(instruction.result, lower_unary(instruction));
                break;
            case binary:
                bind(instruction.result, lower_binary(instruction));
                break;
            case cast:
                bind(instruction.result, cast_value(value(instruction.operands.front()), instruction.type));
                break;
            case aggregate_undef:
                bind(instruction.result, llvm::UndefValue::get(types.lower(instruction.type)));
                break;
            case insert_value:
                bind(
                    instruction.result,
                    builder.CreateInsertValue(
                        value(instruction.operands[0]),
                        value(instruction.operands[1]),
                        llvm_indices(instruction.indices)
                    )
                );
                break;
            case extract_value:
                bind(
                    instruction.result,
                    builder.CreateExtractValue(value(instruction.operands.front()), llvm_indices(instruction.indices))
                );
                break;
            case alloc_raw:
                bind(instruction.result, lower_alloc_raw(instruction));
                break;
            case free_raw:
                builder.CreateCall(cp_free_function(), { value(instruction.operands.front()) });
                break;
            case call:
                bind_call(instruction);
                break;
            case branch:
                builder.CreateBr(blocks[instruction.targets.front().value]);
                break;
            case cond_branch:
                builder.CreateCondBr(
                    condition(value(instruction.operands.front())),
                    blocks[instruction.targets[0].value],
                    blocks[instruction.targets[1].value]
                );
                break;
            case return_:
                if(instruction.operands.empty()) {
                    builder.CreateRetVoid();
                } else {
                    builder.CreateRet(value(instruction.operands.front()));
                }
                break;
        }
    }

    auto lower_literal(
        semantic_type_id type,
        semantic_literal_value const& literal,
        symbol_id symbol
    ) -> llvm::Value*
    {
        if(symbol.valid()) {
            return functions[symbol];
        }
        return std::visit (
            overloaded {
                [&](std::monostate) -> llvm::Value* {
                    auto lowered = types.lower(type);
                    if(lowered->isVoidTy()) {
                        return nullptr;
                    }
                    return llvm::Constant::getNullValue(lowered);
                },
                [&](bool value) -> llvm::Value* {
                    return llvm::ConstantInt::get(types.lower(type), value ? 1 : 0);
                },
                [&](std::int64_t value) -> llvm::Value* {
                    return llvm::ConstantInt::get(types.lower(type), value, true);
                },
                [&](double value) -> llvm::Value* {
                    return llvm::ConstantFP::get(types.lower(type), value);
                },
                [&](char value) -> llvm::Value* {
                    return llvm::ConstantInt::get(types.lower(type), static_cast<unsigned char>(value), false);
                },
                [&](std::string const& value) -> llvm::Value* {
                    return builder.CreateGlobalString(value);
                },
            },
            literal.value
        );
    }

    auto lower_unary(ir_instruction const& instruction) -> llvm::Value*
    {
        auto operand = value(instruction.operands.front());
        using enum token_kind;
        switch(instruction.operator_kind) {
            case plus:
                return operand;
            case minus:
                if(operand->getType()->isFloatingPointTy()) {
                    return builder.CreateFNeg(operand);
                }
                return builder.CreateNeg(operand);
            case tilde:
                return builder.CreateNot(operand);
            case kw_not:
                return bool_to_i8(builder.CreateICmpEQ(condition(operand), llvm::ConstantInt::getFalse(context)));
            case plus_plus:
                return builder.CreateAdd(operand, llvm::ConstantInt::get(operand->getType(), 1));
            case minus_minus:
                return builder.CreateSub(operand, llvm::ConstantInt::get(operand->getType(), 1));
            default:
                return operand;
        }
    }

    auto lower_binary(ir_instruction const& instruction) -> llvm::Value*
    {
        auto left = value(instruction.operands[0]);
        auto right = value(instruction.operands[1]);
        using enum token_kind;
        if(instruction.aggregate_type.valid() and instruction.type.valid()) {
            if(
                instruction.operator_kind == minus
                and instruction.type == semantic_type_ids::builtin(builtin_type_kind::isize)
            ) {
                return builder.CreatePtrDiff(types.lower(instruction.aggregate_type), left, right);
            }
            if(instruction.operator_kind == minus) {
                right = builder.CreateNeg(right);
            }
            return builder.CreateGEP(types.lower(instruction.aggregate_type), left, right);
        }
        switch(instruction.operator_kind) {
            case plus:
                return left->getType()->isFloatingPointTy()
                    ? builder.CreateFAdd(left, right)
                    : builder.CreateAdd(left, right);
            case minus:
                return left->getType()->isFloatingPointTy()
                    ? builder.CreateFSub(left, right)
                    : builder.CreateSub(left, right);
            case star:
                return left->getType()->isFloatingPointTy()
                    ? builder.CreateFMul(left, right)
                    : builder.CreateMul(left, right);
            case slash:
                return left->getType()->isFloatingPointTy()
                    ? builder.CreateFDiv(left, right)
                    : builder.CreateSDiv(left, right);
            case percent:
                return builder.CreateSRem(left, right);
            case pipe:
            case kw_or:
                return builder.CreateOr(left, right);
            case caret:
                return builder.CreateXor(left, right);
            case amp:
            case kw_and:
                return builder.CreateAnd(left, right);
            case less_less:
                return builder.CreateShl(left, right);
            case greater_greater:
                return builder.CreateAShr(left, right);
            case equal_equal:
                return bool_to_i8(compare(left, right, llvm::CmpInst::ICMP_EQ, llvm::CmpInst::FCMP_OEQ));
            case bang_equal:
                return bool_to_i8(compare(left, right, llvm::CmpInst::ICMP_NE, llvm::CmpInst::FCMP_ONE));
            case less:
                return bool_to_i8(compare(left, right, llvm::CmpInst::ICMP_SLT, llvm::CmpInst::FCMP_OLT));
            case less_equal:
                return bool_to_i8(compare(left, right, llvm::CmpInst::ICMP_SLE, llvm::CmpInst::FCMP_OLE));
            case greater:
                return bool_to_i8(compare(left, right, llvm::CmpInst::ICMP_SGT, llvm::CmpInst::FCMP_OGT));
            case greater_equal:
                return bool_to_i8(compare(left, right, llvm::CmpInst::ICMP_SGE, llvm::CmpInst::FCMP_OGE));
            default:
                return llvm::Constant::getNullValue(types.lower(instruction.type));
        }
    }

    auto bind_call(ir_instruction const& instruction) -> void
    {
        auto arguments = std::vector<llvm::Value*>{};
        llvm::Value* callee{};
        llvm::FunctionType* signature{};
        if(instruction.symbol.valid()) {
            callee = functions[instruction.symbol];
            signature = functions[instruction.symbol]->getFunctionType();
            for(auto operand : instruction.operands) {
                arguments.emplace_back(value(operand));
            }
        } else {
            callee = value(instruction.operands.front());
            signature = types.lower_function_signature(instruction.aggregate_type);
            for(auto operand : instruction.operands | std::views::drop(1uz)) {
                arguments.emplace_back(value(operand));
            }
        }
        auto call = builder.CreateCall(signature, callee, arguments);
        if(instruction.result.valid() and not call->getType()->isVoidTy()) {
            bind(instruction.result, call);
        }
    }

    auto lower_alloc_raw(ir_instruction const& instruction) -> llvm::Value*
    {
        auto count = cast_value(value(instruction.operands.front()), semantic_type_ids::builtin(builtin_type_kind::u64));
        auto elem_size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), type_size(instruction.aggregate_type));
        auto align = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), type_align(instruction.aggregate_type));
        return builder.CreateCall(cp_alloc_function(), { elem_size, align, count });
    }

    auto cp_alloc_function() -> llvm::Function*
    {
        if(auto function = llvm_module.getFunction("cp_alloc")) {
            return function;
        }

        auto i64 = llvm::Type::getInt64Ty(context);
        auto pointer = llvm::PointerType::get(context, 0);
        auto function_type = llvm::FunctionType::get(pointer, { i64, i64, i64 }, false);
        return llvm::Function::Create(
            function_type,
            llvm::Function::ExternalLinkage,
            "cp_alloc",
            llvm_module
        );
    }

    auto cp_free_function() -> llvm::Function*
    {
        if(auto function = llvm_module.getFunction("cp_free")) {
            return function;
        }

        auto pointer = llvm::PointerType::get(context, 0);
        auto function_type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), { pointer }, false);
        return llvm::Function::Create(
            function_type,
            llvm::Function::ExternalLinkage,
            "cp_free",
            llvm_module
        );
    }

    auto type_size(semantic_type_id id) -> std::uint64_t
    {
        auto const& kind = ir.types.get(id);
        return std::visit (
            overloaded {
                [](unit_type const&) -> std::uint64_t { return 0; },
                [](error_type const&) -> std::uint64_t { return 0; },
                [](inferred_type const&) -> std::uint64_t { return 0; },
                [&](builtin_type const& type) { return builtin_size(type.kind); },
                [&](array_type const& type) { return type_size(type.element) * type.length; },
                [&](tuple_type const& type) -> std::uint64_t {
                    auto total = std::uint64_t{};
                    auto alignment = 1uz;
                    for(auto element : type.elements) {
                        auto element_alignment = type_align(element);
                        alignment = std::max(alignment, element_alignment);
                        total = align_to(total, element_alignment);
                        total += type_size(element);
                    }
                    return align_to(total, alignment);
                },
                [](reference_type const&) -> std::uint64_t { return 8; },
                [](pointer_type const&) -> std::uint64_t { return 8; },
                [](function_type const&) -> std::uint64_t { return 8; },
                [](generic_parameter_type const&) -> std::uint64_t { return 0; },
                [&](struct_type const& type) -> std::uint64_t {
                    auto total = std::uint64_t{};
                    auto alignment = 1uz;
                    for(auto const& field : ir.structs[type.index].fields) {
                        auto field_alignment = type_align_substituted(field.type, type.arguments);
                        alignment = std::max(alignment, field_alignment);
                        total = align_to(total, field_alignment);
                        total += type_size_substituted(field.type, type.arguments);
                    }
                    return align_to(total, alignment);
                },
                [&](variant_type const& type) -> std::uint64_t {
                    auto total = std::uint64_t{4};
                    auto alignment = 4uz;
                    for(auto const& variant_case : ir.variants[type.index].cases) {
                        auto payload_size = variant_case_payload_size(variant_case, type.arguments);
                        auto payload_alignment = variant_case_payload_align(variant_case, type.arguments);
                        alignment = std::max(alignment, payload_alignment);
                        total = align_to(total, payload_alignment);
                        total += payload_size;
                    }
                    return align_to(total, alignment);
                },
            },
            kind
        );
    }

    auto type_size_substituted(
        semantic_type_id id,
        std::vector<semantic_type_id> const& arguments
    ) -> std::uint64_t
    {
        auto const& kind = ir.types.get(id);
        if(auto const* parameter = std::get_if<generic_parameter_type>(&kind)) {
            if(parameter->index < arguments.size()) {
                return type_size(arguments[parameter->index]);
            }
            return 0;
        }
        return type_size(id);
    }

    auto variant_case_payload_size(
        semantic_variant_case const& variant_case,
        std::vector<semantic_type_id> const& arguments
    ) -> std::uint64_t
    {
        auto total = std::uint64_t{};
        auto alignment = 1uz;
        for(auto payload : variant_case.payload_types) {
            auto payload_alignment = type_align_substituted(payload, arguments);
            alignment = std::max(alignment, payload_alignment);
            total = align_to(total, payload_alignment);
            total += type_size_substituted(payload, arguments);
        }
        return align_to(total, alignment);
    }

    auto type_align(semantic_type_id id) -> std::uint64_t
    {
        auto const& kind = ir.types.get(id);
        return std::visit (
            overloaded {
                [](unit_type const&) -> std::uint64_t { return 1; },
                [](error_type const&) -> std::uint64_t { return 1; },
                [](inferred_type const&) -> std::uint64_t { return 1; },
                [&](builtin_type const& type) { return builtin_align(type.kind); },
                [&](array_type const& type) { return type_align(type.element); },
                [&](tuple_type const& type) -> std::uint64_t {
                    auto result = 1uz;
                    for(auto element : type.elements) {
                        result = std::max(result, type_align(element));
                    }
                    return result;
                },
                [](reference_type const&) -> std::uint64_t { return 8; },
                [](pointer_type const&) -> std::uint64_t { return 8; },
                [](function_type const&) -> std::uint64_t { return 8; },
                [](generic_parameter_type const&) -> std::uint64_t { return 1; },
                [&](struct_type const& type) -> std::uint64_t {
                    auto result = 1uz;
                    for(auto const& field : ir.structs[type.index].fields) {
                        result = std::max(result, type_align_substituted(field.type, type.arguments));
                    }
                    return result;
                },
                [&](variant_type const& type) -> std::uint64_t {
                    auto result = 4uz;
                    for(auto const& variant_case : ir.variants[type.index].cases) {
                        for(auto payload : variant_case.payload_types) {
                            result = std::max(result, type_align_substituted(payload, type.arguments));
                        }
                    }
                    return result;
                },
            },
            kind
        );
    }

    auto type_align_substituted(
        semantic_type_id id,
        std::vector<semantic_type_id> const& arguments
    ) -> std::uint64_t
    {
        auto const& kind = ir.types.get(id);
        if(auto const* parameter = std::get_if<generic_parameter_type>(&kind)) {
            if(parameter->index < arguments.size()) {
                return type_align(arguments[parameter->index]);
            }
            return 1;
        }
        return type_align(id);
    }

    auto variant_case_payload_align(
        semantic_variant_case const& variant_case,
        std::vector<semantic_type_id> const& arguments
    ) -> std::uint64_t
    {
        auto result = 1uz;
        for(auto payload : variant_case.payload_types) {
            result = std::max(result, type_align_substituted(payload, arguments));
        }
        return result;
    }

    auto builtin_size(builtin_type_kind kind) -> std::uint64_t
    {
        using enum builtin_type_kind;
        switch(kind) {
            case bool_:
            case i8:
            case u8:
            case char_:
                return 1uz;
            case i16:
            case u16:
                return 2uz;
            case i32:
            case u32:
            case f32:
                return 4uz;
            case i64:
            case u64:
            case isize:
            case usize:
            case f64:
            case str:
                return 8uz;
        }
        std::unreachable();
    }

    auto builtin_align(builtin_type_kind kind) -> std::uint64_t
    {
        return std::min<std::uint64_t>(builtin_size(kind), 8);
    }

    auto align_to(std::uint64_t value, std::uint64_t alignment) -> std::uint64_t
    {
        return ((value + alignment - 1uz) / alignment) * alignment;
    }

    auto llvm_indices(std::vector<std::uint64_t> const& values) -> std::vector<unsigned>
    {
        auto result = std::vector<unsigned>{};
        result.reserve(values.size());
        for(auto value : values) {
            result.emplace_back(static_cast<unsigned>(value));
        }
        return result;
    }

    auto compare(
        llvm::Value* left,
        llvm::Value* right,
        llvm::CmpInst::Predicate int_predicate,
        llvm::CmpInst::Predicate float_predicate
    ) -> llvm::Value*
    {
        if(left->getType()->isFloatingPointTy()) {
            return builder.CreateFCmp(float_predicate, left, right);
        }
        return builder.CreateICmp(int_predicate, left, right);
    }

    auto cast_value(llvm::Value* value, semantic_type_id target) -> llvm::Value*
    {
        auto target_type = types.lower(target);
        if(value->getType() == target_type) {
            return value;
        }
        if(value->getType()->isIntegerTy() and target_type->isIntegerTy()) {
            return builder.CreateIntCast(value, target_type, true);
        }
        if(value->getType()->isIntegerTy() and target_type->isFloatingPointTy()) {
            return builder.CreateSIToFP(value, target_type);
        }
        if(value->getType()->isFloatingPointTy() and target_type->isIntegerTy()) {
            return builder.CreateFPToSI(value, target_type);
        }
        if(value->getType()->isFloatingPointTy() and target_type->isFloatingPointTy()) {
            return builder.CreateFPCast(value, target_type);
        }
        return builder.CreateBitCast(value, target_type);
    }

    auto condition(llvm::Value* value) -> llvm::Value*
    {
        if(value->getType()->isIntegerTy(1)) {
            return value;
        }
        if(value->getType()->isIntegerTy()) {
            return builder.CreateICmpNE(value, llvm::ConstantInt::get(value->getType(), 0));
        }
        if(value->getType()->isFloatingPointTy()) {
            return builder.CreateFCmpONE(value, llvm::ConstantFP::get(value->getType(), 0.0));
        }
        return builder.CreateIsNotNull(value);
    }

    auto bool_to_i8(llvm::Value* value) -> llvm::Value*
    {
        return builder.CreateZExt(value, llvm::Type::getInt8Ty(context));
    }

    auto emit_default_return(semantic_type_id type) -> void
    {
        auto return_type = types.lower(type);
        if(return_type->isVoidTy()) {
            builder.CreateRetVoid();
        } else {
            builder.CreateRet(llvm::Constant::getNullValue(return_type));
        }
    }

    auto value(ir_value_id id) -> llvm::Value*
    {
        return values[id];
    }

    auto bind(ir_value_id id, llvm::Value* value) -> void
    {
        if(value == nullptr) {
            return;
        }
        values.emplace(id, value);
    }

    llvm::LLVMContext context;
    llvm::Module llvm_module;
    ir_module const& ir;
    llvm_type_lowerer types;
    llvm::IRBuilder<> builder;
    std::map<symbol_id, llvm::Function*> functions{};
    std::map<ir_value_id, llvm::Value*> values{};
    std::vector<llvm::BasicBlock*> blocks{};
};

export auto emit_llvm_ir(ir_module const& ir) -> llvm_emit_result
{
    auto lowerer = llvm_module_lowerer{ ir };
    return lowerer.lower();
}
