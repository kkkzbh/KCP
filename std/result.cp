export module std.result;

export variant result<T,E> {
    ok(T);
    error(E);
}

impl result<T,E> {
    is_ok(self const&) -> bool
    {
        return match self {
            .ok(value) => true,
            .error(err) => false,
        };
    }

    is_error(self const&) -> bool
    {
        return not self.is_ok();
    }

    value_or(self const&, fallback: T) -> T
    {
        return match self {
            .ok(value) => value,
            .error(err) => fallback,
        };
    }
}
