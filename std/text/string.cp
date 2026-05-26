export module std.text.string;

import std.memory.span;
import std.core.option;
import std.text.detail.string_storage;
import std.text.str;
export import std.core.iter;

export struct string {
    storage: string_storage;
    len: usize;
}

export struct string_iter {
    current: char*;
    end: char*;
}

impl string {
    string()
    {
        return string{ .storage = string_storage{}, .len = 0 };
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
        let result = string{ .storage = other.storage.take(), .len = other.len };
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

        storage.replace(move rhs.storage);
        len = rhs.len;

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
        return storage.capacity();
    }

    empty(self const&) -> bool
    {
        return len == 0;
    }

    operator [](self like&, index: usize) -> char like&
    {
        assert(index < len, "string index out of bounds");
        return ref *(storage.data() + index);
    }

    front(self like&) -> char like&
    {
        assert(len != 0, "string front on empty string");
        return ref *storage.data();
    }

    back(self like&) -> char like&
    {
        assert(len != 0, "string back on empty string");
        return ref *(storage.data() + len - 1);
    }

    reserve(self&, new_capacity: usize) -> void
    {
        if(new_capacity <= capacity()) {
            return;
        }

        let next = string_storage{new_capacity};
        let index: usize = 0;
        while(index < len) {
            *(next.data() + index) = *(storage.data() + index);
            index += 1;
        }

        storage.replace(move next);
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
        assert(len != 0, "string pop_back on empty string");
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
        storage.terminate(len);
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

impl contiguous_mutable_range for string {
    type item = char;
}
