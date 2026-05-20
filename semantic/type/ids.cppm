export module semantic.type:ids;

import std;
import :builtin;

export struct [[nodiscard]] semantic_type_id
{
    constexpr semantic_type_id() = default;

    explicit constexpr semantic_type_id(std::uint32_t raw) :
        value(raw) {}

    auto constexpr valid() const -> bool
    {
        return value != invalid_value;
    }

    auto constexpr operator==(semantic_type_id const&) const -> bool = default;

    auto constexpr operator<=>(semantic_type_id const&) const = default;

    auto constexpr static invalid_value = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t value{ invalid_value };
};

export namespace semantic_type_ids {
auto constexpr unit = semantic_type_id{ 0 };
auto constexpr error = semantic_type_id{ 1 };
auto constexpr inferred = semantic_type_id{ 2 };
auto constexpr never = semantic_type_id{ 3 };

auto constexpr builtin_offset = 4u;
auto constexpr builtin_count = static_cast<std::uint32_t>(builtin_type_count);
auto constexpr fixed_count = builtin_offset + builtin_count;

auto constexpr builtin(builtin_type_kind kind) -> semantic_type_id
{
    return semantic_type_id{ builtin_offset + builtin_type_index(kind) };
}

auto constexpr bool_ = builtin(builtin_type_kind::bool_);
auto constexpr i32 = builtin(builtin_type_kind::i32);
auto constexpr f64 = builtin(builtin_type_kind::f64);
auto constexpr char_ = builtin(builtin_type_kind::char_);
auto constexpr str = builtin(builtin_type_kind::str);
auto constexpr nullptr_ = builtin(builtin_type_kind::nullptr_);
} // namespace semantic_type_ids

export auto constexpr is_error(semantic_type_id id) -> bool
{
    return id == semantic_type_ids::error;
}

export auto constexpr is_inferred(semantic_type_id id) -> bool
{
    return id == semantic_type_ids::inferred;
}

export auto constexpr is_unit(semantic_type_id id) -> bool
{
    return id == semantic_type_ids::unit;
}

export auto constexpr is_never(semantic_type_id id) -> bool
{
    return id == semantic_type_ids::never;
}

export auto constexpr is_nullptr(semantic_type_id id) -> bool
{
    return id == semantic_type_ids::nullptr_;
}

export auto constexpr as_builtin(semantic_type_id id) -> std::optional<builtin_type_kind>
{
    if(id.value < semantic_type_ids::builtin_offset or id.value >= semantic_type_ids::fixed_count) {
        return std::nullopt;
    }
    return builtin_type_at(id.value - semantic_type_ids::builtin_offset);
}
