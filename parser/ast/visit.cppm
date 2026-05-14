export module parser.ast.visit;

import std;
import parser.ast.expr;
import parser.ast.stmt;

export template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};

export template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

export template <class Node, class Variant>
auto is(Variant const& value) -> bool
{
    return std::holds_alternative<Node>(value);
}

export template <class Node, class Variant>
auto as(Variant& value) -> Node&
{
    return std::get<Node>(value);
}

export template <class Node, class Variant>
auto as(Variant const& value) -> Node const&
{
    return std::get<Node>(value);
}
