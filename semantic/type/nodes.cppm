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

export struct never_type
{
    auto constexpr operator==(never_type const&) const -> bool = default;
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
    semantic_type_id length{};
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

    enum class kind : std::uint8_t
    {
        regular,
        like,
        move,
        forward,
    };

    explicit constexpr reference_type(semantic_type_id type_pointee, bool target_const = false, kind reference_kind = kind::regular) :
        pointee(type_pointee),
        is_const(target_const),
        reference_kind(reference_kind) {}

    auto constexpr operator==(reference_type const&) const -> bool = default;

    semantic_type_id pointee{};
    bool is_const{};
    kind reference_kind{ kind::regular };
};

export struct pointer_type
{
    constexpr pointer_type() = default;

    explicit constexpr pointer_type(semantic_type_id type_pointee, bool target_const = false, bool target_like = false) :
        pointee(type_pointee),
        is_const(target_const),
        is_like(target_like) {}

    auto constexpr operator==(pointer_type const&) const -> bool = default;

    semantic_type_id pointee{};
    bool is_const{};
    bool is_like{};
};

export struct function_type
{
    auto constexpr operator==(function_type const&) const -> bool = default;

    std::vector<semantic_type_id> parameters{};
    semantic_type_id returns{};
};

export struct generic_parameter_type
{
    auto constexpr operator==(generic_parameter_type const&) const -> bool = default;

    std::uint32_t index{};
};

export struct associated_type_ref
{
    auto constexpr operator==(associated_type_ref const&) const -> bool = default;

    semantic_type_id owner{};
    std::string name{};
};

export struct integer_constant_type
{
    auto constexpr operator==(integer_constant_type const&) const -> bool = default;

    std::int64_t value{};
    builtin_type_kind type{ builtin_type_kind::usize };
};

export struct generic_integer_parameter_type
{
    auto constexpr operator==(generic_integer_parameter_type const&) const -> bool = default;

    std::uint32_t index{};
    builtin_type_kind type{ builtin_type_kind::usize };
};

export struct struct_type
{
    auto constexpr operator==(struct_type const&) const -> bool = default;

    std::uint32_t index{};
    std::vector<semantic_type_id> arguments{};
};

export struct enum_type
{
    auto constexpr operator==(enum_type const&) const -> bool = default;

    std::uint32_t index{};
};

export struct opaque_type
{
    auto constexpr operator==(opaque_type const&) const -> bool = default;

    std::uint32_t index{};
};

export struct variant_type
{
    auto constexpr operator==(variant_type const&) const -> bool = default;

    std::uint32_t index{};
    std::vector<semantic_type_id> arguments{};
};

export using semantic_type_kind = std::variant <
    unit_type,
    error_type,
    inferred_type,
    never_type,
    builtin_type,
    array_type,
    tuple_type,
    reference_type,
    pointer_type,
    function_type,
    generic_parameter_type,
    associated_type_ref,
    integer_constant_type,
    generic_integer_parameter_type,
    struct_type,
    enum_type,
    opaque_type,
    variant_type
>;
