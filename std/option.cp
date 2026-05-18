export module std.option;

export variant optional<T> {
    none;
    some(T);
}
