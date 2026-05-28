export module std.collections.set;

import std.collections.detail.btree;
import std.collections.detail.btree_storage;
import std.compare;
import std.core.iter;
import std.core.option;

export struct set_node<K> {
    key: K;
}

export struct set_node_ref<K> {
    key: K&;
}

export struct set_insert_result<K> {
    node: set_node_ref<K>;
    inserted: bool;
}

export struct set<K, Order: ordering<K> = asc<K>> {
    tree: btree<K, btree_empty, Order>;
}

impl set<K, Order> {
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
        return tree.find_key(key) != nullptr;
    }

    find(self like&, key: K const&) -> optional<K like&>
    {
        let found = tree.find_key(key);
        if(found == nullptr) {
            return optional<K like&>::none;
        }
        return optional<K like&>::some(ref *found);
    }

    at(self like&, key: K const&) -> K like&
    {
        let found = tree.find_key(key);
        assert(found != nullptr, "set at missing key");
        return ref *found;
    }

    insert(self&, key: K) -> set_insert_result<K>
    {
        let result = tree.insert_unique(move key, btree_empty{});
        return set_insert_result<K>{
            .node = set_node_ref<K>{ .key = ref *result.item.key },
            .inserted = result.inserted
        };
    }

    insert_node(self&, node: set_node<K>) -> set_insert_result<K>
    {
        let result = tree.insert_unique(move node.key, btree_empty{});
        return set_insert_result<K>{
            .node = set_node_ref<K>{ .key = ref *result.item.key },
            .inserted = result.inserted
        };
    }

    remove(self&, key: K const&) -> bool
    {
        return tree.remove(key);
    }

    nth(self like&, index: usize) -> K like&
    {
        assert(index < tree.size(), "set nth index out of bounds");
        return ref *tree.nth_key(index);
    }

    nth_node(self like&, index: usize) -> set_node<K>
    {
        assert(index < tree.size(), "set nth_node index out of bounds");
        return set_node<K>{ .key = *tree.nth_key(index) };
    }

    rank(self const&, key: K const&) -> usize
    {
        return tree.rank(key);
    }
}

export struct set_iter<K, Order: ordering<K> = asc<K>> {
    source: set<K, Order>*;
    index: usize;
}

export struct const_set_iter<K, Order: ordering<K> = asc<K>> {
    source: set<K, Order> const*;
    index: usize;
}

impl<K, Order: ordering<K>> iterator for set_iter<K, Order> {
    type iter_item = K&;

    next(self&) -> optional<K&>
    {
        if(index >= (*source).size()) {
            return optional<K&>::none;
        }

        let current = index;
        index += 1;
        return optional<K&>::some(ref (*source).nth(current));
    }
}

impl<K, Order: ordering<K>> iterator for const_set_iter<K, Order> {
    type iter_item = K const&;

    next(self&) -> optional<K const&>
    {
        if(index >= (*source).size()) {
            return optional<K const&>::none;
        }

        let current = index;
        index += 1;
        return optional<K const&>::some(const ref (*source).nth(current));
    }
}

impl<K, Order: ordering<K>> iterable for set<K, Order> {
    type iter_type = set_iter<K, Order>;
    type iter_item = K&;

    iter(self&) -> set_iter<K, Order>
    {
        return set_iter<K, Order>{ .source = &self, .index = 0 as usize };
    }
}

impl<K, Order: ordering<K>> const_iterable for set<K, Order> {
    type const_iter_type = const_set_iter<K, Order>;
    type const_iter_item = K const&;

    iter(self const&) -> const_set_iter<K, Order>
    {
        return const_set_iter<K, Order>{ .source = &self, .index = 0 as usize };
    }
}
