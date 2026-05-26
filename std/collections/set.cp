export module std.collections.set;

import std.collections.detail.btree;
import std.compare;
import std.core.option;

export struct set_node<K> {
    key: K;
}

export struct set_insert_result<K> {
    node: set_node<K>&;
    inserted: bool;
}

struct set_traits<K> {
}

export struct set<K, Compare: strict_weak_order<K> = less<K>> {
    tree: btree<K, set_node<K>, set_traits<K>, Compare>;
}

impl set_traits<K> {
    key(self const&, item: set_node<K> const&) -> K const&
    {
        return item.key;
    }
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
        return tree.find_item(key) != nullptr;
    }

    find(self like&, key: K const&) -> optional<K like&>
    {
        let node = tree.find_item(key);
        if(node == nullptr) {
            return optional<K like&>::none;
        }
        return optional<K like&>::some(ref (*node).key);
    }

    at(self like&, key: K const&) -> K like&
    {
        let node = tree.find_item(key);
        assert(node != nullptr, "set at missing key");
        return ref (*node).key;
    }

    insert(self&, key: K) -> set_insert_result<K>
    {
        let result = tree.insert_unique(set_node<K>{ .key = move key });
        return set_insert_result<K>{ .node = ref *result.item, .inserted = result.inserted };
    }

    insert_node(self&, node: set_node<K>) -> set_insert_result<K>
    {
        let result = tree.insert_unique(move node);
        return set_insert_result<K>{ .node = ref *result.item, .inserted = result.inserted };
    }

    remove(self&, key: K const&) -> bool
    {
        return tree.remove(key);
    }

    nth(self like&, index: usize) -> K like&
    {
        assert(index < tree.size(), "set nth index out of bounds");
        let node = tree.nth_item(index);
        return ref (*node).key;
    }

    nth_node(self like&, index: usize) -> set_node<K> like&
    {
        assert(index < tree.size(), "set nth_node index out of bounds");
        let node = tree.nth_item(index);
        return ref *node;
    }

    rank(self const&, key: K const&) -> usize
    {
        return tree.rank(key);
    }
}
