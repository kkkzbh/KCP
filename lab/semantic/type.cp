export module semantic.type;

export enum semantic_type_kind : u8 {
    error = 0;
    int_type = 1;
    void_type = 2;
}

export semantic_type_name(kind: semantic_type_kind) -> str
{
    if(kind == semantic_type_kind::int_type) { return "int"; }
    if(kind == semantic_type_kind::void_type) { return "void"; }
    return "error";
}

export semantic_type_is_value(kind: semantic_type_kind) -> bool
{
    return kind == semantic_type_kind::int_type;
}
