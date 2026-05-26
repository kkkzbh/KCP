export module std.collections.set;

import std.collections.detail.btree;
import std.collections.detail.btree_storage;
import std.compare;
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
