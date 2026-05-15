export module semantic.type:builtin;

import std;

export enum class builtin_type_kind : std::uint8_t
{
    bool_,
    i8,
    i16,
    i32,
    i64,
    u8,
    u16,
    u32,
    u64,
    f32,
    f64,
    char_,
    str,
};

struct builtin_type_info
{
    builtin_type_kind kind{};
    std::string_view name{};
};

auto constexpr builtin_types = std::array{
    builtin_type_info{ builtin_type_kind::bool_, "bool" },
    builtin_type_info{ builtin_type_kind::i8, "i8" },
    builtin_type_info{ builtin_type_kind::i16, "i16" },
    builtin_type_info{ builtin_type_kind::i32, "i32" },
    builtin_type_info{ builtin_type_kind::i64, "i64" },
    builtin_type_info{ builtin_type_kind::u8, "u8" },
    builtin_type_info{ builtin_type_kind::u16, "u16" },
    builtin_type_info{ builtin_type_kind::u32, "u32" },
    builtin_type_info{ builtin_type_kind::u64, "u64" },
    builtin_type_info{ builtin_type_kind::f32, "f32" },
    builtin_type_info{ builtin_type_kind::f64, "f64" },
    builtin_type_info{ builtin_type_kind::char_, "char" },
    builtin_type_info{ builtin_type_kind::str, "str" },
};

export auto constexpr builtin_type_count = builtin_types.size();

export auto constexpr builtin_type_index(builtin_type_kind kind) -> std::uint32_t
{
    auto found = std::ranges::find_if(builtin_types, [&](auto const& type) {
        return type.kind == kind;
    });
    return static_cast<std::uint32_t>(std::distance(builtin_types.begin(), found));
}

export auto constexpr builtin_type_at(std::uint32_t index) -> builtin_type_kind
{
    return builtin_types[index].kind;
}

export auto type_name(builtin_type_kind kind) -> std::string_view
{
    auto found = std::ranges::find_if(builtin_types, [&](auto const& type) {
        return type.kind == kind;
    });
    if(found != builtin_types.end()) {
        return found->name;
    }
    std::unreachable();
}

export auto constexpr is_builtin_name(std::string_view name) -> std::optional<builtin_type_kind>
{
    auto found = std::ranges::find_if(builtin_types, [&](auto const& type) {
        return type.name == name;
    });
    if(found != builtin_types.end()) {
        return found->kind;
    }
    return std::nullopt;
}

export auto is_integer(builtin_type_kind kind) -> bool
{
    using enum builtin_type_kind;
    return kind == i8 or kind == i16 or kind == i32 or kind == i64
           or kind == u8 or kind == u16 or kind == u32 or kind == u64;
}

export auto is_float(builtin_type_kind kind) -> bool
{
    using enum builtin_type_kind;
    return kind == f32 or kind == f64;
}

export auto is_numeric(builtin_type_kind kind) -> bool
{
    return is_integer(kind) or is_float(kind);
}

export auto integer_rank(builtin_type_kind kind) -> int
{
    using enum builtin_type_kind;
    switch(kind) {
        case i8:
        case u8:
            return 1;
        case i16:
        case u16:
            return 2;
        case i32:
        case u32:
            return 3;
        case i64:
        case u64:
            return 4;
        default:
            return 0;
    }
}

export auto float_rank(builtin_type_kind kind) -> int
{
    using enum builtin_type_kind;
    switch(kind) {
        case f32: return 1;
        case f64: return 2;
        default: return 0;
    }
}
