export module std.memory.raw_buffer;

export struct raw_buffer<T> {
    ptr: T*;
    cap: usize;
}

impl raw_buffer<T> {
    raw_buffer() = default;

    raw_buffer(capacity: usize)
    {
        if(capacity == 0) {
            return raw_buffer<T>{ .ptr = nullptr, .cap = 0 };
        }
        return raw_buffer<T>{ .ptr = alloc<T>(capacity), .cap = capacity };
    }

    raw_buffer(other: this const&) = delete;
    operator =(self&, rhs: this const&) = delete;
    operator =(self&, rhs: this move&) = delete;

    raw_buffer(other: this move&)
    {
        return other.take();
    }

    ~raw_buffer()
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

    take(self&) -> raw_buffer<T>
    {
        let result = raw_buffer<T>{ .ptr = ptr, .cap = cap };
        ptr = nullptr;
        cap = 0;
        return result;
    }

    reset(self&) -> void
    {
        free(ptr);
        ptr = nullptr;
        cap = 0;
    }

    replace(self&, next: this move&) -> void
    {
        reset();
        ptr = next.ptr;
        cap = next.cap;
        next.ptr = nullptr;
        next.cap = 0;
    }

    swap(self&, other: raw_buffer<T>&) -> void
    {
        let old_ptr = ptr;
        let old_cap = cap;
        ptr = other.ptr;
        cap = other.cap;
        other.ptr = old_ptr;
        other.cap = old_cap;
    }
}
