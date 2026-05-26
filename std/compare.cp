export module std.compare;

export concept mutable_object {
}

export variant partial_ordering {
    less;
    equivalent;
    greater;
    unordered;
}

export variant weak_ordering {
    less;
    equivalent;
    greater;
}

export variant strong_ordering {
    less;
    equivalent;
    greater;
}

export concept ordering<T> {
    operator ()(self const&, left: T const&, right: T const&) -> weak_ordering;
}

export concept equality_comparable<Rhs = this> {
    operator ==(self const&, rhs: Rhs const&) -> bool;
}

export concept three_way_comparable<Rhs = this, Category = weak_ordering> {
    operator <=>(self const&, rhs: Rhs const&) -> Category;
}

export concept incrementable {
    operator prefix ++(self&) -> this&;
}

export is_less(value: weak_ordering) -> bool
{
    return match value {
        .less => true,
        .equivalent => false,
        .greater => false,
    };
}

export is_equivalent(value: weak_ordering) -> bool
{
    return match value {
        .less => false,
        .equivalent => true,
        .greater => false,
    };
}

export is_greater(value: weak_ordering) -> bool
{
    return match value {
        .less => false,
        .equivalent => false,
        .greater => true,
    };
}

export reverse_order(value: weak_ordering) -> weak_ordering
{
    return match value {
        .less => weak_ordering::greater,
        .equivalent => weak_ordering::equivalent,
        .greater => weak_ordering::less,
    };
}

export struct asc<T> {
}

export struct desc<T> {
}

impl<T> asc<T> {
    operator ()(self const&, left: T const&, right: T const&) -> weak_ordering
    {
        return (left <=> right).to_weak();
    }
}

impl<T> desc<T> {
    operator ()(self const&, left: T const&, right: T const&) -> weak_ordering
    {
        return reverse_order(asc<T>{}(left, right));
    }
}

impl weak_ordering {
    to_weak(self) -> weak_ordering
    {
        return self;
    }
}

impl strong_ordering {
    to_weak(self) -> weak_ordering
    {
        return match self {
            .less => weak_ordering::less,
            .equivalent => weak_ordering::equivalent,
            .greater => weak_ordering::greater,
        };
    }
}

compare_builtin<T>(left: T const&, right: T const&) -> weak_ordering
{
    if(left < right) {
        return weak_ordering::less;
    }
    if(right < left) {
        return weak_ordering::greater;
    }
    return weak_ordering::equivalent;
}

impl bool {
    operator <=>(self const&, rhs: bool const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}

impl i8 {
    operator <=>(self const&, rhs: i8 const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}

impl i16 {
    operator <=>(self const&, rhs: i16 const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}

impl i32 {
    operator <=>(self const&, rhs: i32 const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}

impl i64 {
    operator <=>(self const&, rhs: i64 const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}

impl u8 {
    operator <=>(self const&, rhs: u8 const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}

impl u16 {
    operator <=>(self const&, rhs: u16 const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}

impl u32 {
    operator <=>(self const&, rhs: u32 const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}

impl u64 {
    operator <=>(self const&, rhs: u64 const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}

impl isize {
    operator <=>(self const&, rhs: isize const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}

impl usize {
    operator <=>(self const&, rhs: usize const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}

impl char {
    operator <=>(self const&, rhs: char const&) -> weak_ordering
    {
        return compare_builtin(self, rhs);
    }
}
