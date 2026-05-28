export module std.text.str;

import std.core.iter;
import std.core.option;
import std.compare;

impl str {
    size(self) -> usize
    {
        return len;
    }

    data(self) -> char const*
    {
        return ptr;
    }

    operator <=>(self const&, rhs: this const&) -> weak_ordering
    {
        let index = 0 as usize;
        while(index < len and index < rhs.len) {
            if(ptr[index] < rhs.ptr[index]) {
                return weak_ordering::less;
            }
            if(ptr[index] > rhs.ptr[index]) {
                return weak_ordering::greater;
            }
            index += 1;
        }
        if(len < rhs.len) {
            return weak_ordering::less;
        }
        if(len > rhs.len) {
            return weak_ordering::greater;
        }
        return weak_ordering::equivalent;
    }

    operator ==(self const&, rhs: this const&) -> bool
    {
        if(len != rhs.len) {
            return false;
        }
        let index = 0 as usize;
        while(index < len) {
            if(ptr[index] != rhs.ptr[index]) {
                return false;
            }
            index += 1;
        }
        return true;
    }

    operator !=(self const&, rhs: this const&) -> bool
    {
        return not (self == rhs);
    }

    operator <(self const&, rhs: this const&) -> bool
    {
        let index = 0 as usize;
        while(index < len and index < rhs.len) {
            if(ptr[index] < rhs.ptr[index]) {
                return true;
            }
            if(ptr[index] > rhs.ptr[index]) {
                return false;
            }
            index += 1;
        }
        return len < rhs.len;
    }

    operator <=(self const&, rhs: this const&) -> bool
    {
        return not (rhs < self);
    }

    operator >(self const&, rhs: this const&) -> bool
    {
        return rhs < self;
    }

    operator >=(self const&, rhs: this const&) -> bool
    {
        return not (self < rhs);
    }
}

export struct str_iter {
    text: str;
    index: usize;
}

impl iterator for str_iter {
    type iter_item = char;

    next(self&) -> optional<char>
    {
        if(index >= text.size()) {
            return optional<char>::none;
        }
        let ch = text[index];
        index += 1;
        return optional<char>::some(ch);
    }
}

impl iterable for str {
    type iter_type = str_iter;
    type iter_item = char;

    iter(self&) -> str_iter
    {
        return str_iter{ .text = self, .index = 0 };
    }
}

impl const_iterable for str {
    type const_iter_type = str_iter;
    type const_iter_item = char;

    iter(self const&) -> str_iter
    {
        return str_iter{ .text = self, .index = 0 };
    }
}
