export module std.option;

export variant optional<T> {
    none;
    some(T);
}

impl optional<T> {
    is_some(self: optional<T> const&) -> bool
    {
        return match self {
            .some(value) => true,
            .none => false,
        };
    }

    is_none(self: optional<T> const&) -> bool
    {
        return not self.is_some();
    }

    value_or(self: optional<T> const&, fallback: T) -> T
    {
        return match self {
            .some(value) => value,
            .none => fallback,
        };
    }
}
