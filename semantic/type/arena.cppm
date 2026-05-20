export module semantic.type:arena;

import std;
import :builtin;
import :ids;
import :nodes;

export struct type_arena
{
    auto intern(semantic_type_kind kind) -> semantic_type_id
    {
        if(auto fixed = fixed_id(kind)) {
            return *fixed;
        }

        if(auto dynamic = dynamic_id(kind)) {
            return *dynamic;
        }

        auto id = semantic_type_id{ semantic_type_ids::fixed_count + static_cast<std::uint32_t>(dynamic_types.size()) };
        dynamic_types.emplace_back(std::move(kind));
        return id;
    }

    auto get(semantic_type_id id) const -> semantic_type_kind const&
    {
        if(id.value < semantic_type_ids::fixed_count) {
            return fixed_types()[id.value];
        }
        return dynamic_types[id.value - semantic_type_ids::fixed_count];
    }

    auto contains(semantic_type_id id) const -> bool
    {
        if(id.value < semantic_type_ids::fixed_count) {
            return true;
        }
        return id.value - semantic_type_ids::fixed_count < dynamic_types.size();
    }

private:
    auto static fixed_id(semantic_type_kind const& kind) -> std::optional<semantic_type_id>
    {
        if(std::holds_alternative<unit_type>(kind)) {
            return semantic_type_ids::unit;
        }
        if(std::holds_alternative<error_type>(kind)) {
            return semantic_type_ids::error;
        }
        if(std::holds_alternative<inferred_type>(kind)) {
            return semantic_type_ids::inferred;
        }
        if(std::holds_alternative<never_type>(kind)) {
            return semantic_type_ids::never;
        }
        if(auto const* builtin = std::get_if<builtin_type>(&kind)) {
            return semantic_type_ids::builtin(builtin->kind);
        }
        return std::nullopt;
    }

    auto dynamic_id(semantic_type_kind const& kind) const -> std::optional<semantic_type_id>
    {
        if(not std::ranges::contains(dynamic_types, kind)) {
            return std::nullopt;
        }

        auto found = std::ranges::find(dynamic_types, kind);
        return semantic_type_id{
            semantic_type_ids::fixed_count
            + static_cast<std::uint32_t>(std::distance(dynamic_types.begin(), found))
        };
    }

    auto static fixed_types() -> std::array<semantic_type_kind, semantic_type_ids::fixed_count> const&
    {
        auto static const values = std::array{
            semantic_type_kind{ unit_type{} },
            semantic_type_kind{ error_type{} },
            semantic_type_kind{ inferred_type{} },
            semantic_type_kind{ never_type{} },
            semantic_type_kind{ builtin_type{ builtin_type_kind::bool_ } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::i8 } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::i16 } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::i32 } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::i64 } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::u8 } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::u16 } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::u32 } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::u64 } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::isize } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::usize } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::f32 } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::f64 } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::char_ } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::str } },
            semantic_type_kind{ builtin_type{ builtin_type_kind::nullptr_ } },
        };
        return values;
    }

    std::vector<semantic_type_kind> dynamic_types{};
};
