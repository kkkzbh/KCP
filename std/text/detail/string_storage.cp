export module std.text.detail.string_storage;

import std.memory.raw_buffer;

export struct string_storage {
    raw: raw_buffer<char>;
}

impl string_storage {
    string_storage()
    {
        let result = string_storage{ .raw = raw_buffer<char>{1} };
        result.terminate(0 as usize);
        return result;
    }

    string_storage(capacity: usize)
    {
        let result = string_storage{ .raw = raw_buffer<char>{capacity + 1} };
        result.terminate(0 as usize);
        return result;
    }

    string_storage(other: this const&) = delete;
    operator =(self&, rhs: this const&) = delete;
    operator =(self&, rhs: this move&) = delete;

    string_storage(other: this move&)
    {
        return other.take();
    }

    data(self like&) -> char like*
    {
        return raw.data();
    }

    capacity(self const&) -> usize
    {
        if(raw.capacity() == 0) {
            return 0;
        }
        return raw.capacity() - 1;
    }

    physical_capacity(self const&) -> usize
    {
        return raw.capacity();
    }

    empty(self const&) -> bool
    {
        return capacity() == 0;
    }

    terminate(self&, len: usize) -> void
    {
        if(raw.capacity() == 0) {
            return;
        }
        *(raw.data() + len) = '\0';
    }

    take(self&) -> string_storage
    {
        return string_storage{ .raw = raw.take() };
    }

    replace(self&, next: this move&) -> void
    {
        raw.replace(move next.raw);
    }

    swap(self&, other: string_storage&) -> void
    {
        raw.swap(ref other.raw);
    }
}
