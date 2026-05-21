export module std.compare;

export concept mutable_object {
}

export concept strict_weak_order<T> {
    operator ()(self const&, left: T const&, right: T const&) -> bool;
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

export concept equality_comparable<Rhs = this> {
    operator ==(self const&, rhs: Rhs const&) -> bool;
}

export concept three_way_comparable<Rhs = this, Category = weak_ordering> {
    operator <=>(self const&, rhs: Rhs const&) -> Category;
}

export concept incrementable {
    operator prefix ++(self&) -> this&;
}

export struct less<T> {
}

export struct greater<T> {
}

impl<T> less<T> {
    operator ()(self const&, left: T const&, right: T const&) -> bool
    {
        return left < right;
    }
}

impl<T> greater<T> {
    operator ()(self const&, left: T const&, right: T const&) -> bool
    {
        return left > right;
    }
}
