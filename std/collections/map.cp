export module std.collections.map;

import std.collections.detail.btree;
import std.compare;
import std.core.option;

export struct map_node<K, V> {
    key: K;
    value: V;
}

export struct map_insert_result<K, V> {
    node: map_node<K, V>&;
    inserted: bool;
}

struct map_traits<K, V> {
}

export struct map<K, V, Compare: strict_weak_order<K> = less<K>> {
    tree: btree<K, map_node<K, V>, map_traits<K, V>, Compare>;
}

impl map_traits<K, V> {
    key(self const&, item: map_node<K, V> const&) -> K const&
    {
        return item.key;
    }
}

impl map<K, V, Compare> {
    map() = default;

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

    find(self like&, key: K const&) -> optional<V like&>
    {
        let node = tree.find_item(key);
        if(node == nullptr) {
            return optional<V like&>::none;
        }
        return optional<V like&>::some(ref (*node).value);
    }

    at(self like&, key: K const&) -> V like&
    {
        let node = tree.find_item(key);
        assert(node != nullptr, "map at missing key");
        return ref (*node).value;
    }

    operator [](self&, key: K) -> V&
    {
        let node = tree.find_item(key);
        if(node != nullptr) {
            return ref (*node).value;
        }

        let inserted = tree.insert_unique(map_node<K, V>{ .key = move key, .value = V{} });
        return ref (*inserted.item).value;
    }

    insert(self&, key: K, value: V) -> map_insert_result<K, V>
    {
        let result = tree.insert_unique(map_node<K, V>{ .key = move key, .value = move value });
        return map_insert_result<K, V>{ .node = ref *result.item, .inserted = result.inserted };
    }

    insert_node(self&, node: map_node<K, V>) -> map_insert_result<K, V>
    {
        let result = tree.insert_unique(move node);
        return map_insert_result<K, V>{ .node = ref *result.item, .inserted = result.inserted };
    }

    remove(self&, key: K const&) -> bool
    {
        return tree.remove(key);
    }

    nth(self like&, index: usize) -> map_node<K, V> like&
    {
        assert(index < tree.size(), "map nth index out of bounds");
        let node = tree.nth_item(index);
        return ref *node;
    }

    rank(self const&, key: K const&) -> usize
    {
        return tree.rank(key);
    }
}
