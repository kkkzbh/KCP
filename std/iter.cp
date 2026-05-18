export module std.iter;

export import std.option;

export concept iterator {
    type iter_item;

    next(self: Self&) -> optional<iter_item>;
}

export concept iterable {
    type iter_type;
    type iter_item;

    requires (
        iter_type: iterator
        and iter_type::iter_item == iter_item
    );

    iter(self: Self&) -> iter_type;
}
