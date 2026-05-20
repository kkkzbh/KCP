export module std.string;

import std.buffer;
import std.detail.runtime;
import std.option;
import std.str;
export import std.iter;

export struct string {
    storage: buffer<char>;
    len: usize;
}

export struct string_iter {
    current: char*;
    end: char*;
}

impl string {
    string()
    {
        let result = string{ .storage = buffer<char>{1}, .len = 0 };
        result.terminate();
        return result;
    }

    string(text: str)
    {
        let result = string{};
        result.append(text);
        return result;
    }

    string(other: this const&)
    {
        let result = string{};
        result.append(other.as_str());
        return result;
    }

    string(other: this move&)
    {
        let result = string{ .storage = buffer<char>{ .ptr = other.storage.ptr, .cap = other.storage.cap }, .len = other.len };
        other.storage.ptr = nullptr;
        other.storage.cap = 0;
        other.len = 0;
        return result;
    }

    operator =(self&, rhs: this const&) -> this&
    {
        if(&rhs == &self) {
            return ref self;
        }

        clear();
        append(rhs.as_str());
        return ref self;
    }

    operator =(self&, rhs: this move&) -> this&
    {
        if(&rhs == &self) {
            return ref self;
        }

        free(storage.data());
        storage.ptr = rhs.storage.ptr;
        storage.cap = rhs.storage.cap;
        len = rhs.len;

        rhs.storage.ptr = nullptr;
        rhs.storage.cap = 0;
        rhs.len = 0;
        return ref self;
    }

    data(self like&) -> char like*
    {
        return storage.data();
    }

    begin(self like&) -> char like*
    {
        return storage.data();
    }

    end(self like&) -> char like*
    {
        return storage.data() + len;
    }

    as_str(self const&) -> str
    {
        return str{ .ptr = storage.data(), .len = len };
    }

    size(self const&) -> usize
    {
        return len;
    }

    capacity(self const&) -> usize
    {
        if(storage.capacity() == 0) {
            return 0;
        }
        return storage.capacity() - 1;
    }

    empty(self const&) -> bool
    {
        return len == 0;
    }

    operator [](self like&, index: usize) -> char like&
    {
        if(index >= len) {
            cp_bounds_fail();
        }

        return ref *(storage.data() + index);
    }

    front(self like&) -> char like&
    {
        if(len == 0) {
            cp_bounds_fail();
        }

        return ref *storage.data();
    }

    back(self like&) -> char like&
    {
        if(len == 0) {
            cp_bounds_fail();
        }

        return ref *(storage.data() + len - 1);
    }

    reserve(self&, new_capacity: usize) -> void
    {
        if(new_capacity <= capacity()) {
            return;
        }

        let next = buffer<char>{new_capacity + 1};
        let index: usize = 0;
        while(index < len) {
            *(next.data() + index) = *(storage.data() + index);
            index += 1;
        }

        free(storage.data());
        storage.ptr = next.ptr;
        storage.cap = next.cap;
        next.ptr = nullptr;
        next.cap = 0;
        terminate();
    }

    clear(self&) -> void
    {
        len = 0;
        terminate();
    }

    push_back(self&, ch: char) -> void
    {
        ensure_capacity(len + 1);
        *(storage.data() + len) = ch;
        len += 1;
        terminate();
    }

    pop_back(self&) -> void
    {
        if(len == 0) {
            cp_bounds_fail();
        }

        len -= 1;
        terminate();
    }

    resize(self&, new_size: usize, ch: char) -> void
    {
        if(new_size < len) {
            len = new_size;
            terminate();
            return;
        }

        ensure_capacity(new_size);
        while(len < new_size) {
            *(storage.data() + len) = ch;
            len += 1;
        }
        terminate();
    }

    append(self&, text: str) -> void
    {
        let text_size = text.size();
        let self_append = text.data() == data();
        ensure_capacity(len + text_size);

        let source = text.data();
        if(self_append) {
            source = data();
        }

        let index: usize = 0;
        while(index < text_size) {
            *(storage.data() + len + index) = *(source + index);
            index += 1;
        }
        len += text_size;
        terminate();
    }

    ensure_capacity(self&, required: usize) -> void
    {
        if(required <= capacity()) {
            return;
        }

        let next_capacity = capacity() * 2;
        if(next_capacity == 0) {
            next_capacity = 1;
        }
        if(next_capacity < required) {
            next_capacity = required;
        }
        reserve(next_capacity);
    }

    terminate(self&) -> void
    {
        if(storage.capacity() == 0) {
            return;
        }
        *(storage.data() + len) = '\0';
    }
}

impl iterator for string_iter {
    type iter_item = char&;

    next(self&) -> optional<char&>
    {
        if(current >= end) {
            return optional<char&>::none;
        }

        let ch = current;
        current = current + 1;
        return optional<char&>::some(ref *ch);
    }
}

impl iterable for string {
    type iter_type = string_iter;
    type iter_item = char&;

    iter(self&) -> string_iter
    {
        return string_iter{ .current = data(), .end = data() + len };
    }
}
