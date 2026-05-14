export module semantic.check.keys;

import std;
import source;
import parser.ast.ids;

export struct return_inference_key
{
    constexpr return_inference_key() = default;

    explicit constexpr return_inference_key(std::size_t unit, function_id id) :
        unit_index(unit),
        function_id_value(id.value) {}

    auto constexpr operator==(return_inference_key const&) const -> bool = default;
    auto constexpr operator<=>(return_inference_key const&) const = default;

    std::size_t unit_index{};
    std::uint32_t function_id_value{};
};

export struct parameter_binding_key
{
    constexpr parameter_binding_key() = default;

    explicit constexpr parameter_binding_key(std::size_t unit, byte_pos position) :
        unit_index(unit),
        name_start(position) {}

    auto constexpr operator==(parameter_binding_key const&) const -> bool = default;
    auto constexpr operator<=>(parameter_binding_key const&) const = default;

    std::size_t unit_index{};
    byte_pos name_start{};
};
