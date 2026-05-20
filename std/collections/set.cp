export module std.collections.set;

import std.collections.detail.rb_tree;
import std.compare;
import std.core.option;

export struct set_node<K> {
    key: K;
}

export struct set_insert_result<K> {
    node: set_node<K>&;
    inserted: bool;
}

export struct set<K, Compare: strict_weak_order<K> = less<K>> {
    tree: rb_tree<K, set_node<K>, Compare>;
}

impl set<K, Compare> {
    set() = default;

    size(self const&) -> usize
    {
        return tree.size();
    }

    empty(self const&) -> bool
    {
        return tree.empty();
    }

    clear(self&) -> void
    {
        tree.clear();
    }

    contains(self const&, key: K const&) -> bool
    {
        return tree.find_node(key) != nullptr;
    }

    find(self like&, key: K const&) -> optional<K like&>
    {
        let node = tree.find_node(key);
        if(node == nullptr) {
            return optional<K like&>::none;
        }
        return optional<K like&>::some(ref (*node).value.key);
    }

    at(self like&, key: K const&) -> K like&
    {
        let node = tree.find_node(key);
        assert(node != nullptr, "set at missing key");
        return ref (*node).value.key;
    }

    insert(self&, key: K) -> set_insert_result<K>
    {
        let result = tree.insert_unique(set_node<K>{ .key = move key });
        return set_insert_result<K>{ .node = ref (*result.node).value, .inserted = result.inserted };
    }

    insert_node(self&, node: set_node<K>) -> set_insert_result<K>
    {
        let result = tree.insert_unique(move node);
        return set_insert_result<K>{ .node = ref (*result.node).value, .inserted = result.inserted };
    }

    remove(self&, key: K const&) -> bool
    {
        return tree.remove(key);
    }

    nth(self like&, index: usize) -> K like&
    {
        assert(index < tree.size(), "set nth index out of bounds");
        let node = tree.nth_node(index);
        return ref (*node).value.key;
    }

    nth_node(self like&, index: usize) -> set_node<K> like&
    {
        assert(index < tree.size(), "set nth_node index out of bounds");
        let node = tree.nth_node(index);
        return ref (*node).value;
    }

    rank(self const&, key: K const&) -> usize
    {
        return tree.rank(key);
    }
}
