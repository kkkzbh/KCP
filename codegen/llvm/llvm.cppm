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
            [&](sequence_type const& type) -> llvm::Type* {
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
                auto parameters = std::vector<llvm::Type*>{};
                for(auto parameter : type.parameters) {
                    parameters.emplace_back(lower(parameter));
                }
                return llvm::FunctionType::get(lower(type.returns), parameters, false);
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
                    return llvm::Constant::getNullValue(types.lower(type));
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
        for(auto operand : instruction.operands) {
            arguments.emplace_back(value(operand));
        }
        auto call = builder.CreateCall(functions[instruction.symbol], arguments);
        if(instruction.result.valid() and not call->getType()->isVoidTy()) {
            bind(instruction.result, call);
        }
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
