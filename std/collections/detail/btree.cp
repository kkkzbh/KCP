export module std.collections.detail.btree;

import std.collections.detail.btree_storage;
import std.compare;

export struct btree_insert_result<K, V> {
    item: btree_item_ref<K, V>;
    inserted: bool;
}

export struct btree_position {
    node: u32;
    index: usize;
}

export struct btree<K, V, Order: ordering<K> = asc<K>> {
    root: u32;
    len: usize;
    pool: btree_node_pool<K, V>;
    order: Order;
}

impl btree<K, V, Order> {
    btree()
    {
        return btree<K, V, Order>{
            .root = btree_invalid_node_id(),
            .len = 0,
            .pool = btree_node_pool<K, V>{},
            .order = Order{}
        };
    }

    btree(other: this const&)
    {
        let result = btree<K, V, Order>{
            .root = btree_invalid_node_id(),
            .len = 0,
            .pool = btree_node_pool<K, V>{},
            .order = other.order
        };
        result.clone_from(other);
        return result;
    }

    btree(other: this move&)
    {
        let result = btree<K, V, Order>{
            .root = other.root,
            .len = other.len,
            .pool = move other.pool,
            .order = move other.order
        };
        other.root = btree_invalid_node_id();
        other.len = 0;
        return result;
    }

    ~btree()
    {
        clear();
    }

    operator =(self&, rhs: this const&) -> this&
    {
        if(&rhs == &self) {
            return ref self;
        }

        clear();
        order = rhs.order;
        clone_from(rhs);
        return ref self;
    }

    operator =(self&, rhs: this move&) -> this&
    {
        if(&rhs == &self) {
            return ref self;
        }

        clear();
        root = rhs.root;
        len = rhs.len;
        pool.chunks = move rhs.pool.chunks;
        pool.allocated = rhs.pool.allocated;
        pool.free_head = rhs.pool.free_head;
        order = move rhs.order;
        rhs.root = btree_invalid_node_id();
        rhs.len = 0;
        rhs.pool.allocated = 0;
        rhs.pool.free_head = btree_invalid_node_id();
        return ref self;
    }

    clear(self&) -> void
    {
        pool.clear();
        root = btree_invalid_node_id();
        len = 0;
    }

    size(self const&) -> usize
    {
        return len;
    }

    empty(self const&) -> bool
    {
        return len == 0;
    }

    find_position(self const&, key: K const&) -> btree_position
    {
        let current = root;
        while(current != btree_invalid_node_id()) {
            let index = lower_bound_in_node(current, key);
            let target = &pool.node(current);
            if(index < (*target).len and keys_equal(key, pool.key(current, index))) {
                return btree_position{ .node = current, .index = index };
            }
            if((*target).leaf) {
                return btree_position{ .node = btree_invalid_node_id(), .index = 0 as usize };
            }
            current = pool.child(current, index);
        }
        return btree_position{ .node = btree_invalid_node_id(), .index = 0 as usize };
    }

    find_key(self like&, key: K const&) -> K like*
    {
        let position = find_position(key);
        if(position.node == btree_invalid_node_id()) {
            return nullptr;
        }
        return pool.key_slot(position.node, position.index);
    }

    find_value(self like&, key: K const&) -> V like*
    {
        let position = find_position(key);
        if(position.node == btree_invalid_node_id()) {
            return nullptr;
        }
        return pool.value_slot(position.node, position.index);
    }

    nth_position(self const&, index: usize) -> btree_position
    {
        assert(index < len, "btree nth_item index out of bounds");
        let current = root;
        let remaining = index;
        while(current != btree_invalid_node_id()) {
            let target = &pool.node(current);
            let slot: usize = 0;
            while(slot < (*target).len) {
                let child_size: usize = 0;
                if(not (*target).leaf) {
                    child_size = pool.node(*((*target).children + slot)).subtree_size;
                }
                if(remaining < child_size) {
                    current = *((*target).children + slot);
                    break;
                }
                remaining -= child_size;
                if(remaining == 0) {
                    return btree_position{ .node = current, .index = slot };
                }
                remaining -= 1;
                slot += 1;
            }
            if(slot < (*target).len) {
                continue;
            }
            assert(not (*target).leaf, "btree nth_item traversal exhausted leaf");
            current = pool.child(current, (*target).len);
        }
        return btree_position{ .node = btree_invalid_node_id(), .index = 0 as usize };
    }

    nth_item(self&, index: usize) -> btree_item_ref<K, V>
    {
        let position = nth_position(index);
        assert(position.node != btree_invalid_node_id(), "btree nth_item missing positioned item");
        return pool.item(position.node, position.index);
    }

    nth_key(self like&, index: usize) -> K like*
    {
        let position = nth_position(index);
        if(position.node == btree_invalid_node_id()) {
            return nullptr;
        }
        return pool.key_slot(position.node, position.index);
    }

    nth_value(self like&, index: usize) -> V like*
    {
        let position = nth_position(index);
        if(position.node == btree_invalid_node_id()) {
            return nullptr;
        }
        return pool.value_slot(position.node, position.index);
    }

    rank(self const&, key: K const&) -> usize
    {
        let current = root;
        let result: usize = 0;
        while(current != btree_invalid_node_id()) {
            let target = &pool.node(current);
            let index: usize = 0;
            while(index < (*target).len) {
                let current_key = pool.key(current, index);
                let relation = order(current_key, key);
                if(is_greater(relation)) {
                    if((*target).leaf) {
                        return result;
                    }
                    current = *((*target).children + index);
                    break;
                }
                if(not (*target).leaf) {
                    result += pool.node(*((*target).children + index)).subtree_size;
                }
                if(is_less(relation)) {
                    result += 1;
                    index += 1;
                } else {
                    return result;
                }
            }
            if(index < (*target).len) {
                continue;
            }
            if((*target).leaf) {
                return result;
            }
            current = *((*target).children + (*target).len);
        }
        return result;
    }

    insert_unique(self&, key: K, value: V) -> btree_insert_result<K, V>
    {
        if(root == btree_invalid_node_id()) {
            root = pool.allocate(true);
            pool.insert_item(root, 0, move key, move value);
            len = 1;
            pool.node(root).subtree_size = 1;
            return btree_insert_result<K, V>{ .item = pool.item(root, 0), .inserted = true };
        }

        if(pool.node(root).len == btree_node_capacity()) {
            let index = lower_bound_in_node(root, key);
            let target = &pool.node(root);
            if(index < (*target).len and keys_equal(key, pool.key(root, index))) {
                return btree_insert_result<K, V>{ .item = pool.item(root, index), .inserted = false };
            }

            let old_root = root;
            root = pool.allocate(false);
            pool.set_child(root, 0, old_root);
            split_child_for_insert(root, 0, index);
        }

        return insert_non_full(root, move key, move value);
    }

    insert_non_full(self&, id: u32, key: K, value: V) -> btree_insert_result<K, V>
    {
        let target = &pool.node(id);
        let index = lower_bound_in_node(id, key);
        if(index < (*target).len and keys_equal(key, pool.key(id, index))) {
            return btree_insert_result<K, V>{ .item = pool.item(id, index), .inserted = false };
        }

        if((*target).leaf) {
            pool.insert_item(id, index, move key, move value);
            len += 1;
            (*target).subtree_size += 1;
            return btree_insert_result<K, V>{ .item = pool.item(id, index), .inserted = true };
        }

        let child_index = index;
        let child_id = pool.child(id, child_index);
        if(pool.node(child_id).len == btree_node_capacity()) {
            let child_insert_index = lower_bound_in_node(child_id, key);
            let child = &pool.node(child_id);
            if(child_insert_index < (*child).len and keys_equal(key, pool.key(child_id, child_insert_index))) {
                return btree_insert_result<K, V>{ .item = pool.item(child_id, child_insert_index), .inserted = false };
            }

            split_child_for_insert(id, child_index, child_insert_index);
            let promoted_key = pool.key(id, child_index);
            let relation = order(key, promoted_key);
            if(is_less(relation)) {
                child_id = pool.child(id, child_index);
            } else if(is_greater(relation)) {
                child_id = pool.child(id, child_index + 1);
            } else {
                return btree_insert_result<K, V>{ .item = pool.item(id, child_index), .inserted = false };
            }
        }

        let result = insert_non_full(child_id, move key, move value);
        if(result.inserted) {
            (*target).subtree_size += 1;
        }
        return result;
    }

    remove(self&, key: K const&) -> bool
    {
        if(root == btree_invalid_node_id()) {
            return false;
        }

        let removed = remove_from_node(root, key);
        if(not removed) {
            return false;
        }

        len -= 1;
        if(len == 0) {
            pool.release(root);
            root = btree_invalid_node_id();
            return true;
        }

        let target = &pool.node(root);
        if(not (*target).leaf and (*target).len == 0) {
            let old_root = root;
            root = pool.child(old_root, 0);
            pool.release(old_root);
        }
        return true;
    }

    clone_from(self&, other: this const&) -> void
    {
        if(other.root == btree_invalid_node_id()) {
            return;
        }
        clone_node_items(other, other.root);
    }

    clone_node_items(self&, other: this const&, id: u32) -> void
    {
        let target = &other.pool.node(id);
        let index: usize = 0;
        while(index < (*target).len) {
            if(not (*target).leaf) {
                clone_node_items(other, other.pool.child(id, index));
            }
            insert_unique(other.pool.key(id, index), other.pool.value(id, index));
            index += 1;
        }
        if(not (*target).leaf) {
            clone_node_items(other, other.pool.child(id, (*target).len));
        }
    }

    keys_equal(self const&, left: K const&, right: K const&) -> bool
    {
        return is_equivalent(order(left, right));
    }

    lower_bound_in_node(self const&, id: u32, key: K const&) -> usize
    {
        let target = &pool.node(id);
        let index: usize = 0;
        while(index < (*target).len and is_less(order(pool.key(id, index), key))) {
            index += 1;
        }
        return index;
    }

    recompute_node_size(self&, id: u32) -> void
    {
        if(id == btree_invalid_node_id()) {
            return;
        }

        let target = &pool.node(id);
        let total = (*target).len;
        if(not (*target).leaf) {
            let index: usize = 0;
            while(index <= (*target).len) {
                let child_id = *((*target).children + index);
                if(child_id != btree_invalid_node_id()) {
                    total += pool.node(child_id).subtree_size;
                }
                index += 1;
            }
        }
        (*target).subtree_size = total;
    }

    split_index_for_insert(self const&, child_id: u32, insertion_index: usize) -> usize
    {
        let child = &pool.node(child_id);
        let center = btree_min_items();
        if(not (*child).leaf) {
            return center;
        }
        if(insertion_index < center) {
            return center - 1;
        }
        if(insertion_index <= center + 1) {
            return center;
        }
        return center + 1;
    }

    split_child_for_insert(self&, parent_id: u32, child_index: usize, insertion_index: usize) -> void
    {
        let child_id = pool.child(parent_id, child_index);
        split_child(parent_id, child_index, split_index_for_insert(child_id, insertion_index));
    }

    split_child(self&, parent_id: u32, child_index: usize, median: usize) -> void
    {
        let child_id = pool.child(parent_id, child_index);
        let child = &pool.node(child_id);
        assert((*child).len == btree_node_capacity(), "btree split_child requires a full child");
        assert(median < btree_node_capacity(), "btree split_child median out of bounds");

        let sibling_id = pool.allocate((*child).leaf);
        let right_count = btree_node_capacity() - median - 1;

        if(not (*child).leaf) {
            let index: usize = 0;
            while(index <= right_count) {
                pool.set_child(sibling_id, index, pool.child(child_id, median + 1 + index));
                pool.set_child(child_id, median + 1 + index, btree_invalid_node_id());
                index += 1;
            }
        }

        let index: usize = 0;
        while(index < right_count) {
            let item = pool.remove_item(child_id, median + 1);
            pool.append_item(sibling_id, move item.key, move item.value);
            index += 1;
        }

        let median_item = pool.remove_item(child_id, median);
        pool.insert_child(parent_id, child_index + 1, sibling_id);
        pool.insert_item(parent_id, child_index, move median_item.key, move median_item.value);

        recompute_node_size(child_id);
        recompute_node_size(sibling_id);
        recompute_node_size(parent_id);
    }

    remove_from_node(self&, id: u32, key: K const&) -> bool
    {
        let target = &pool.node(id);
        let index = lower_bound_in_node(id, key);
        if(index < (*target).len and keys_equal(key, pool.key(id, index))) {
            if((*target).leaf) {
                pool.remove_item(id, index);
                recompute_node_size(id);
                return true;
            }

            let left_id = pool.child(id, index);
            let right_id = pool.child(id, index + 1);
            if(pool.node(left_id).len > btree_min_items()) {
                let replacement = remove_max(left_id);
                pool.replace_item(id, index, move replacement);
                recompute_node_size(id);
                return true;
            }
            if(pool.node(right_id).len > btree_min_items()) {
                let replacement = remove_min(right_id);
                pool.replace_item(id, index, move replacement);
                recompute_node_size(id);
                return true;
            }

            merge_children(id, index);
            let merged_id = pool.child(id, index);
            let removed = remove_from_node(merged_id, key);
            recompute_node_size(id);
            return removed;
        }

        if((*target).leaf) {
            return false;
        }

        let child_index = ensure_child_for_remove(id, index);
        let child_id = pool.child(id, child_index);
        let removed = remove_from_node(child_id, key);
        recompute_node_size(id);
        return removed;
    }

    ensure_child_for_remove(self&, parent_id: u32, index: usize) -> usize
    {
        let child_id = pool.child(parent_id, index);
        if(pool.node(child_id).len > btree_min_items()) {
            return index;
        }

        if(index > 0) {
            let left_id = pool.child(parent_id, index - 1);
            if(pool.node(left_id).len > btree_min_items()) {
                borrow_from_left(parent_id, index);
                return index;
            }
        }

        let parent = &pool.node(parent_id);
        if(index < (*parent).len) {
            let right_id = pool.child(parent_id, index + 1);
            if(pool.node(right_id).len > btree_min_items()) {
                borrow_from_right(parent_id, index);
                return index;
            }
            merge_children(parent_id, index);
            return index;
        }

        merge_children(parent_id, index - 1);
        return index - 1;
    }

    remove_min(self&, id: u32) -> btree_removed_item<K, V>
    {
        let target = &pool.node(id);
        if((*target).leaf) {
            let result = pool.remove_item(id, 0);
            recompute_node_size(id);
            return move result;
        }

        let child_index = ensure_child_for_remove(id, 0);
        let result = remove_min(pool.child(id, child_index));
        recompute_node_size(id);
        return move result;
    }

    remove_max(self&, id: u32) -> btree_removed_item<K, V>
    {
        let target = &pool.node(id);
        if((*target).leaf) {
            let result = pool.remove_item(id, (*target).len - 1);
            recompute_node_size(id);
            return move result;
        }

        let child_index = ensure_child_for_remove(id, (*target).len);
        let result = remove_max(pool.child(id, child_index));
        recompute_node_size(id);
        return move result;
    }

    steal_count(self const&, source_len: usize, target_len: usize) -> usize
    {
        let available = source_len - btree_min_items();
        let balanced = (source_len - target_len) / 2;
        if(balanced == 0) {
            return 1;
        }
        if(balanced < available) {
            return balanced;
        }
        return available;
    }

    borrow_from_left(self&, parent_id: u32, index: usize) -> void
    {
        let child_id = pool.child(parent_id, index);
        let left_id = pool.child(parent_id, index - 1);
        let child = &pool.node(child_id);
        let left = &pool.node(left_id);
        let count = steal_count((*left).len, (*child).len);

        let down = pool.take_item(parent_id, index - 1);
        if(not (*child).leaf) {
            let moved_child = pool.remove_child(left_id, (*left).len);
            pool.insert_child(child_id, 0, moved_child);
        }
        pool.insert_item(child_id, 0, move down.key, move down.value);

        let moved: usize = 1;
        while(moved < count) {
            if(not (*child).leaf) {
                let moved_child = pool.remove_child(left_id, pool.node(left_id).len);
                pool.insert_child(child_id, 0, moved_child);
            }
            let item = pool.remove_item(left_id, pool.node(left_id).len - 1);
            pool.insert_item(child_id, 0, move item.key, move item.value);
            moved += 1;
        }

        let up = pool.remove_item(left_id, pool.node(left_id).len - 1);
        pool.construct_item(parent_id, index - 1, move up.key, move up.value);

        recompute_node_size(left_id);
        recompute_node_size(child_id);
        recompute_node_size(parent_id);
    }

    borrow_from_right(self&, parent_id: u32, index: usize) -> void
    {
        let child_id = pool.child(parent_id, index);
        let right_id = pool.child(parent_id, index + 1);
        let child = &pool.node(child_id);
        let right = &pool.node(right_id);
        let count = steal_count((*right).len, (*child).len);

        let down = pool.take_item(parent_id, index);
        if(not (*child).leaf) {
            let moved_child = pool.remove_child(right_id, 0);
            pool.set_child(child_id, (*child).len + 1, moved_child);
        }
        pool.append_item(child_id, move down.key, move down.value);

        let moved: usize = 1;
        while(moved < count) {
            if(not (*child).leaf) {
                let moved_child = pool.remove_child(right_id, 0);
                pool.set_child(child_id, pool.node(child_id).len + 1, moved_child);
            }
            let item = pool.remove_item(right_id, 0);
            pool.append_item(child_id, move item.key, move item.value);
            moved += 1;
        }

        let up = pool.remove_item(right_id, 0);
        pool.construct_item(parent_id, index, move up.key, move up.value);

        recompute_node_size(right_id);
        recompute_node_size(child_id);
        recompute_node_size(parent_id);
    }

    merge_children(self&, parent_id: u32, index: usize) -> void
    {
        let left_id = pool.child(parent_id, index);
        let right_id = pool.child(parent_id, index + 1);
        let left = &pool.node(left_id);
        let right = &pool.node(right_id);
        let left_len = (*left).len;
        let right_len = (*right).len;

        if(not (*left).leaf) {
            let child_index: usize = 0;
            while(child_index <= right_len) {
                pool.set_child(left_id, left_len + 1 + child_index, pool.child(right_id, child_index));
                child_index += 1;
            }
        }

        pool.remove_child(parent_id, index + 1);
        let separator = pool.remove_item(parent_id, index);
        pool.append_item(left_id, move separator.key, move separator.value);

        while(pool.node(right_id).len > 0) {
            let item = pool.remove_item(right_id, 0);
            pool.append_item(left_id, move item.key, move item.value);
        }

        pool.release(right_id);
        recompute_node_size(left_id);
        recompute_node_size(parent_id);
    }
}
