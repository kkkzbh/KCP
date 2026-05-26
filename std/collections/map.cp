export module std.collections.map;

import std.collections.detail.btree;
import std.compare;
import std.core.option;

export struct map_node<K, V> {
    key: K;
    value: V;
}

export struct map_node_ref<K, V> {
    key: K&;
    value: V&;
}

export struct map_insert_result<K, V> {
    node: map_node_ref<K, V>;
    inserted: bool;
}

export struct map<K, V, Order: ordering<K> = asc<K>> {
    tree: btree<K, V, Order>;
}

impl map<K, V, Order> {
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
        return tree.find_key(key) != nullptr;
    }

    find(self like&, key: K const&) -> optional<V like&>
    {
        let value = tree.find_value(key);
        if(value == nullptr) {
            return optional<V like&>::none;
        }
        return optional<V like&>::some(ref *value);
    }

    at(self like&, key: K const&) -> V like&
    {
        let value = tree.find_value(key);
        assert(value != nullptr, "map at missing key");
        return ref *value;
    }

    operator [](self&, key: K) -> V&
    {
        let value = tree.find_value(key);
        if(value != nullptr) {
            return ref *value;
        }

        let inserted = tree.insert_unique(move key, V{});
        return ref *inserted.item.value;
    }

    insert(self&, key: K, value: V) -> map_insert_result<K, V>
    {
        let result = tree.insert_unique(move key, move value);
        return map_insert_result<K, V>{
            .node = map_node_ref<K, V>{ .key = ref *result.item.key, .value = ref *result.item.value },
            .inserted = result.inserted
        };
    }

    insert_node(self&, node: map_node<K, V>) -> map_insert_result<K, V>
    {
        let result = tree.insert_unique(move node.key, move node.value);
        return map_insert_result<K, V>{
            .node = map_node_ref<K, V>{ .key = ref *result.item.key, .value = ref *result.item.value },
            .inserted = result.inserted
        };
    }

    remove(self&, key: K const&) -> bool
    {
        return tree.remove(key);
    }

    nth(self&, index: usize) -> map_node_ref<K, V>
    {
        assert(index < tree.size(), "map nth index out of bounds");
        let item = tree.nth_item(index);
        return map_node_ref<K, V>{ .key = ref *item.key, .value = ref *item.value };
    }

    rank(self const&, key: K const&) -> usize
    {
        return tree.rank(key);
    }
}
