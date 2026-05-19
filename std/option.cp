export module std.option;

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
}
