export module std.collections.map;

import std.collections.detail.btree;
import std.compare;
import std.core.iter;
import std.core.option;

export struct map_node<K, V> {
    key: K;
    value: V;
}

export struct map_node_ref<K, V> {
    key: K&;
    value: V&;
}

export struct map_node_const_ref<K, V> {
    key: K const&;
    value: V const&;
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

    nth(self const&, index: usize) -> map_node_const_ref<K, V>
    {
        assert(index < tree.size(), "map nth index out of bounds");
        return map_node_const_ref<K, V>{ .key = const ref *tree.nth_key(index), .value = const ref *tree.nth_value(index) };
    }

    rank(self const&, key: K const&) -> usize
    {
        return tree.rank(key);
    }
}

export struct map_iter<K, V, Order: ordering<K> = asc<K>> {
    source: map<K, V, Order>*;
    index: usize;
}

export struct const_map_iter<K, V, Order: ordering<K> = asc<K>> {
    source: map<K, V, Order> const*;
    index: usize;
}

impl<K, V, Order: ordering<K>> iterator for map_iter<K, V, Order> {
    type iter_item = map_node_ref<K, V>;

    next(self&) -> optional<map_node_ref<K, V>>
    {
        if(index >= (*source).size()) {
            return optional<map_node_ref<K, V>>::none;
        }

        let current = index;
        index += 1;
        return optional<map_node_ref<K, V>>::some((*source).nth(current));
    }
}

impl<K, V, Order: ordering<K>> iterator for const_map_iter<K, V, Order> {
    type iter_item = map_node_const_ref<K, V>;

    next(self&) -> optional<map_node_const_ref<K, V>>
    {
        if(index >= (*source).size()) {
            return optional<map_node_const_ref<K, V>>::none;
        }

        let current = index;
        index += 1;
        return optional<map_node_const_ref<K, V>>::some((*source).nth(current));
    }
}

impl<K, V, Order: ordering<K>> iterable for map<K, V, Order> {
    type iter_type = map_iter<K, V, Order>;
    type iter_item = map_node_ref<K, V>;

    iter(self&) -> map_iter<K, V, Order>
    {
        return map_iter<K, V, Order>{ .source = &self, .index = 0 as usize };
    }
}

impl<K, V, Order: ordering<K>> const_iterable for map<K, V, Order> {
    type const_iter_type = const_map_iter<K, V, Order>;
    type const_iter_item = map_node_const_ref<K, V>;

    iter(self const&) -> const_map_iter<K, V, Order>
    {
        return const_map_iter<K, V, Order>{ .source = &self, .index = 0 as usize };
    }
}
