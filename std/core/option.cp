export module std.core.option;

export variant optional<T> {
    none;
    some(T);
}

impl optional<T> {
    has_value(self const&) -> bool
    {
        return match self {
            .some(value) => true,
            .none => false,
        };
    }

    value_or(self const&, fallback: T) -> T
    {
        return match self {
            .some(value) => value,
            .none => fallback,
        };
    }

    operator *(self like&) -> T like&
    {
        return match self {
            .some(value) => ref value,
            .none => panic("optional dereference on none"),
        };
    }
}
