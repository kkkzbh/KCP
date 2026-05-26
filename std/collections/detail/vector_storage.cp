export module std.collections.detail.vector_storage;

import std.memory.raw_buffer;

export struct vector_storage<T> {
    raw: raw_buffer<T>;
}

impl vector_storage<T> {
    vector_storage() = default;

    vector_storage(capacity: usize)
    {
        return vector_storage<T>{ .raw = raw_buffer<T>{capacity} };
    }

    vector_storage(other: this const&) = delete;
    operator =(self&, rhs: this const&) = delete;
    operator =(self&, rhs: this move&) = delete;

    vector_storage(other: this move&)
    {
        return other.take();
    }

    data(self like&) -> T like*
    {
        return raw.data();
    }

    capacity(self const&) -> usize
    {
        return raw.capacity();
    }

    empty(self const&) -> bool
    {
        return raw.empty();
    }

    take(self&) -> vector_storage<T>
    {
        return vector_storage<T>{ .raw = raw.take() };
    }

    replace(self&, next: this move&) -> void
    {
        raw.replace(move next.raw);
    }

    swap(self&, other: vector_storage<T>&) -> void
    {
        raw.swap(ref other.raw);
    }
}
