export module std.iter;

import std.option;

export concept iterator {
    type iter_item;

    next(self&) -> optional<iter_item>;
}

export concept iterable {
    type iter_type;
    type iter_item;

    requires (
        iter_type: iterator
        and iter_type::iter_item == iter_item
    );

    iter(self&) -> iter_type;
}
