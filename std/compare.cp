export module std.compare;

export concept mutable_object {
}

export concept strict_weak_order<T> {
    operator ()(self const&, left: T const&, right: T const&) -> bool;
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
