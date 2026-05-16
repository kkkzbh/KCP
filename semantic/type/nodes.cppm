export module semantic.type:nodes;

import std;
import :builtin;
import :ids;

export struct unit_type
{
    auto constexpr operator==(unit_type const&) const -> bool = default;
};

export struct error_type
{
    auto constexpr operator==(error_type const&) const -> bool = default;
};

export struct inferred_type
{
    auto constexpr operator==(inferred_type const&) const -> bool = default;
};

export struct builtin_type
{
    constexpr builtin_type() = default;

    explicit constexpr builtin_type(builtin_type_kind type_kind) :
        kind(type_kind) {}

    auto constexpr operator==(builtin_type const&) const -> bool = default;

    builtin_type_kind kind{};
};

export struct array_type
{
    auto constexpr operator==(array_type const&) const -> bool = default;

    semantic_type_id element{};
    std::uint64_t length{};
};

export struct tuple_type
{
    tuple_type() = default;

    explicit tuple_type(std::vector<semantic_type_id> type_elements) :
        elements(std::move(type_elements)) {}

    auto constexpr operator==(tuple_type const&) const -> bool = default;

    std::vector<semantic_type_id> elements{};
};

export struct reference_type
{
    constexpr reference_type() = default;

    explicit constexpr reference_type(semantic_type_id type_pointee, bool target_const = false) :
        pointee(type_pointee),
        is_const(target_const) {}

    auto constexpr operator==(reference_type const&) const -> bool = default;

    semantic_type_id pointee{};
    bool is_const{};
};

export struct pointer_type
{
    constexpr pointer_type() = default;

    explicit constexpr pointer_type(semantic_type_id type_pointee, bool target_const = false) :
        pointee(type_pointee),
        is_const(target_const) {}

    auto constexpr operator==(pointer_type const&) const -> bool = default;

    semantic_type_id pointee{};
    bool is_const{};
};

export struct function_type
{
    auto constexpr operator==(function_type const&) const -> bool = default;

    std::vector<semantic_type_id> parameters{};
    semantic_type_id returns{};
};

export using semantic_type_kind = std::variant <
    unit_type,
    error_type,
    inferred_type,
    builtin_type,
    array_type,
    tuple_type,
    reference_type,
    pointer_type,
    function_type
>;
