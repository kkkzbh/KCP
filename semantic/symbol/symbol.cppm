export module semantic.symbol;

import std;
import source;
import parser.ast;
import semantic.ty;

export struct [[nodiscard]] symbol_id
{
    constexpr symbol_id() = default;

    explicit constexpr symbol_id(std::uint32_t raw) :
        value(raw) {}

    auto constexpr valid() const -> bool
    {
        return value != invalid_value;
    }

    auto constexpr operator==(symbol_id const&) const -> bool = default;
    auto constexpr operator<=>(symbol_id const&) const = default;

    auto constexpr static invalid_value = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t value{ invalid_value };
};

export struct [[nodiscard]] function_signature_id
{
    constexpr function_signature_id() = default;

    explicit constexpr function_signature_id(std::uint32_t raw) :
        value(raw) {}

    auto constexpr valid() const -> bool
    {
        return value != invalid_value;
    }

    auto constexpr operator==(function_signature_id const&) const -> bool = default;
    auto constexpr operator<=>(function_signature_id const&) const = default;

    auto constexpr static invalid_value = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t value{ invalid_value };
};

export enum class symbol_kind : std::uint8_t
{
    function,
    parameter,
    local,
};

export struct semantic_symbol
{
    auto constexpr operator==(semantic_symbol const&) const -> bool = default;

    symbol_kind kind{};
    std::string name{};
    source_span span{};
    semantic_type_id type{};
    bool exported{};
    bool is_const{};
    std::size_t unit_index{};
    function_id function{};
};

export struct function_signature
{
    auto constexpr operator==(function_signature const&) const -> bool = default;

    std::vector<semantic_type_id> parameters{};
    semantic_type_id returns{};
    bool inferred_return{};
};
