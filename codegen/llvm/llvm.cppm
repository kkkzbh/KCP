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
import parser;
import semantic;

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
        auto const& kind = semantics.types.get(id);
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
    semantic_result const& semantics;
};

export auto emit_llvm_ir(
    source_manager const& sources,
    parse_result const& parsed,
    semantic_result const& semantics
) -> llvm_emit_result
{
    auto context = llvm::LLVMContext{};
    auto module = llvm::Module{ "cp_module", context };
    auto ast_source = ast_source_view{ sources };
    auto lowerer = llvm_type_lowerer {
        .context = context,
        .semantics = semantics,
    };

    if(not parsed.accepted or not parsed.root or not semantics.accepted()) {
        return llvm_emit_result {
            .verified = false,
            .error = "LLVM IR emission requires accepted parser and semantic results",
        };
    }

    auto const& unit = *parsed.root;
    for(auto function_id : unit.functions) {
        auto const& function = parsed.ast.function(function_id);
        auto signature_id = semantics.signature_of(function_id);
        if(not signature_id.valid()) {
            continue;
        }

        auto const& signature = semantics.signatures[signature_id.value];
        auto parameters = std::vector<llvm::Type*>{};
        for(auto parameter : signature.parameters) {
            parameters.emplace_back(lowerer.lower(parameter));
        }

        auto function_type = llvm::FunctionType::get(lowerer.lower(signature.returns), parameters, false);
        auto llvm_function = llvm::Function::Create(
            function_type,
            llvm::Function::ExternalLinkage,
            std::string{ ast_source.slice(function.name) },
            module
        );

        auto block = llvm::BasicBlock::Create(context, "entry", llvm_function);
        auto builder = llvm::IRBuilder<>{ block };
        auto return_type = lowerer.lower(signature.returns);
        if(return_type->isVoidTy()) {
            builder.CreateRetVoid();
        } else {
            builder.CreateRet(llvm::Constant::getNullValue(return_type));
        }
    }

    auto error = std::string{};
    auto error_stream = llvm::raw_string_ostream{ error };
    auto broken = llvm::verifyModule(module, &error_stream);
    auto output = std::string{};
    auto output_stream = llvm::raw_string_ostream{ output };
    module.print(output_stream, nullptr);
    return llvm_emit_result {
        .verified = not broken,
        .ir = output_stream.str(),
        .error = error_stream.str(),
    };
}
