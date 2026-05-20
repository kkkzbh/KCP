export module std.memory.buffer;

export struct buffer<T> {
    ptr: T*;
    cap: usize;
}

impl buffer<T> {
    buffer() = default;

    buffer(capacity: usize)
    {
        if(capacity == 0) {
            return buffer<T>{ .ptr = nullptr, .cap = 0 };
        }
        return buffer<T>{ .ptr = alloc<T>(capacity), .cap = capacity };
    }

    buffer(other: this const&) = delete;
    operator =(self&, rhs: this const&) = delete;
    operator =(self&, rhs: this move&) = delete;

    buffer(other: this move&)
    {
        let result = buffer<T>{ .ptr = other.ptr, .cap = other.cap };
        other.ptr = nullptr;
        other.cap = 0;
        return result;
    }

    ~buffer()
    {
        free(ptr);
    }

    data(self like&) -> T like*
    {
        return ptr;
    }

    capacity(self const&) -> usize
    {
        return cap;
    }

    empty(self const&) -> bool
    {
        return cap == 0;
    }

}
