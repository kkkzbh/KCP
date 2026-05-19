export module std.expected;

export variant expected<T,E> {
    value(T);
    unexpected(E);
}

impl expected<T,E> {
    has_value(self const&) -> bool
    {
        return match self {
            .value(value) => true,
            .unexpected(error) => false,
        };
    }

    value_or(self const&, fallback: T) -> T
    {
        return match self {
            .value(value) => value,
            .unexpected(error) => fallback,
        };
    }
}
