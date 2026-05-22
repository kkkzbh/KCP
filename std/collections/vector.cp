export module std.collections.vector;

import std.memory.buffer;
import std.memory.span;
import std.core.iter;
import std.core.option;

export struct vector<T> {
    storage: buffer<T>;
    len: usize;
}

export struct vector_iter<T> {
    current: T*;
    end: T*;
}

impl vector<T> {
    vector() = default;

    vector(count: usize)
    {
        let result = vector<T>{};
        result.reserve(count);

        let index: usize = 0;
        while(index < count) {
            construct_at(result.storage.data() + index, T{});
            index += 1;
        }
        result.len = count;
        return result;
    }

    vector(count: usize, value: T const&)
    {
        let result = vector<T>{};
        result.reserve(count);

        let index: usize = 0;
        while(index < count) {
            construct_at(result.storage.data() + index, value);
            index += 1;
        }
        result.len = count;
        return result;
    }

    vector(other: this const&)
    {
        let result = vector<T>{};
        result.reserve(other.len);

        let index: usize = 0;
        while(index < other.len) {
            construct_at(result.storage.data() + index, other[index]);
            index += 1;
        }
        result.len = other.len;
        return result;
    }

    vector(other: this move&)
    {
        let result = vector<T>{
            .storage = buffer<T>{ .ptr = other.storage.ptr, .cap = other.storage.cap },
            .len = other.len
        };
        other.storage.ptr = nullptr;
        other.storage.cap = 0;
        other.len = 0;
        return result;
    }

    ~vector()
    {
        clear();
    }

    operator =(self&, rhs: this const&) -> this&
    {
        if(&rhs == &self) {
            return ref self;
        }

        clear();
        reserve(rhs.len);

        let index: usize = 0;
        while(index < rhs.len) {
            construct_at(storage.data() + index, rhs[index]);
            index += 1;
        }
        len = rhs.len;
        return ref self;
    }

    operator =(self&, rhs: this move&) -> this&
    {
        if(&rhs == &self) {
            return ref self;
        }

        clear();
        free(storage.data());

        storage.ptr = rhs.storage.ptr;
        storage.cap = rhs.storage.cap;
        len = rhs.len;

        rhs.storage.ptr = nullptr;
        rhs.storage.cap = 0;
        rhs.len = 0;
        return ref self;
    }

    data(self like&) -> T like*
    {
        return storage.data();
    }

    begin(self like&) -> T like*
    {
        return storage.data();
    }

    end(self like&) -> T like*
    {
        return storage.data() + len;
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

    operator [](self like&, index: usize) -> T like&
    {
        assert(index < len, "vector index out of bounds");
        return ref *(storage.data() + index);
    }

    front(self like&) -> T like&
    {
        assert(len != 0, "vector front on empty vector");
        return ref *storage.data();
    }

    back(self like&) -> T like&
    {
        assert(len != 0, "vector back on empty vector");
        return ref *(storage.data() + len - 1);
    }

    reserve(self&, new_capacity: usize) -> void
    {
        if(new_capacity <= storage.capacity()) {
            return;
        }

        let next = buffer<T>{new_capacity};
        let index: usize = 0;
        while(index < len) {
            construct_at(next.data() + index, move *(storage.data() + index));
            destroy_at(storage.data() + index);
            index += 1;
        }

        free(storage.data());
        storage.ptr = next.ptr;
        storage.cap = next.cap;
        next.ptr = nullptr;
        next.cap = 0;
    }

    clear(self&) -> void
    {
        while(len > 0) {
            len -= 1;
            destroy_at(storage.data() + len);
        }
    }

    push_back(self&, value: T) -> void
    {
        ensure_capacity(len + 1);
        construct_at(storage.data() + len, move value);
        len += 1;
    }

    move_back(self&, value: T move&) -> void
    {
        ensure_capacity(len + 1);
        construct_at(storage.data() + len, move value);
        len += 1;
    }

    pop_back(self&) -> void
    {
        assert(len != 0, "vector pop_back on empty vector");
        len -= 1;
        destroy_at(storage.data() + len);
    }

    pop(self&) -> T
    {
        assert(len != 0, "vector pop on empty vector");
        let index = len - 1;
        let value = move *(storage.data() + index);
        destroy_at(storage.data() + index);
        len = index;
        return move value;
    }

    resize(self&, new_size: usize, value: T const&) -> void
    {
        if(new_size < len) {
            while(len > new_size) {
                self.pop_back();
            }
            return;
        }

        reserve(new_size);
        while(len < new_size) {
            construct_at(storage.data() + len, value);
            len += 1;
        }
    }

    insert(self&, index: usize, value: T) -> void
    {
        assert(index <= len, "vector insert index out of bounds");
        ensure_capacity(len + 1);
        let current = len;
        while(current > index) {
            construct_at(storage.data() + current, move *(storage.data() + current - 1));
            destroy_at(storage.data() + current - 1);
            current -= 1;
        }
        construct_at(storage.data() + index, move value);
        len += 1;
    }

    erase(self&, index: usize) -> void
    {
        assert(index < len, "vector erase index out of bounds");
        destroy_at(storage.data() + index);
        let current = index;
        while(current + 1 < len) {
            construct_at(storage.data() + current, move *(storage.data() + current + 1));
            destroy_at(storage.data() + current + 1);
            current += 1;
        }
        len -= 1;
    }

    erase_range(self&, first: usize, last: usize) -> void
    {
        assert(first <= last and last <= len, "vector erase_range index out of bounds");
        let target = first;
        while(target < last) {
            destroy_at(storage.data() + target);
            target += 1;
        }

        let write = first;
        let read = last;
        while(read < len) {
            construct_at(storage.data() + write, move *(storage.data() + read));
            destroy_at(storage.data() + read);
            write += 1;
            read += 1;
        }
        len -= last - first;
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
}

impl iterator for vector_iter<T> {
    type iter_item = T&;

    next(self&) -> optional<T&>
    {
        if(current >= end) {
            return optional<T&>::none;
        }

        let item = current;
        current = current + 1;
        return optional<T&>::some(ref *item);
    }
}

impl iterable for vector<T> {
    type iter_type = vector_iter<T>;
    type iter_item = T&;

    iter(self&) -> vector_iter<T>
    {
        return vector_iter<T>{ .current = storage.data(), .end = storage.data() + len };
    }
}

impl<T> contiguous_mutable_range for vector<T> {
    type item = T;
}
