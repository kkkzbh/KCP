module semantic.check:state;

import std;
import source;
import parser;
import semantic.ty;
import semantic.symbol;

struct loop_label
{
    std::string name{};
    source_span span{};
};

struct unit_state
{
    parse_result const& parsed;
    std::string module_name{};
    std::map<std::string, symbol_id> visible_functions{};
};

struct return_state
{
    std::optional<semantic_type_id> declared_return{};
};

struct expression_info
{
    semantic_type_id type{};
    bool is_lvalue{};
    bool is_const{};
};

struct aggregate_context
{
    semantic_type_id element{};
    std::uint64_t length{};
};
