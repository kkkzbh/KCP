export module std.collections.detail.btree;

import std.collections.detail.btree_storage;
import std.compare;

export struct btree_insert_result<Item> {
    item: Item*;
    inserted: bool;
}

export struct btree<K, Item, Traits, Compare: strict_weak_order<K> = less<K>> {
    root: u32;
    len: usize;
    pool: btree_node_pool<Item>;
    compare: Compare;
    traits: Traits;
}

impl btree<K, Item, Traits, Compare> {
    btree()
    {
        return btree<K, Item, Traits, Compare>{
            .root = btree_invalid_node_id(),
            .len = 0,
            .pool = btree_node_pool<Item>{},
            .compare = Compare{},
            .traits = Traits{}
        };
    }

    btree(other: this const&)
    {
        let result = btree<K, Item, Traits, Compare>{
            .root = btree_invalid_node_id(),
            .len = 0,
            .pool = btree_node_pool<Item>{},
            .compare = other.compare,
            .traits = other.traits
        };
        result.clone_from(other);
        return result;
    }

    btree(other: this move&)
    {
        let result = btree<K, Item, Traits, Compare>{
            .root = other.root,
            .len = other.len,
            .pool = move other.pool,
            .compare = move other.compare,
            .traits = move other.traits
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
        compare = rhs.compare;
        traits = rhs.traits;
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
        compare = move rhs.compare;
        traits = move rhs.traits;
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

    find_item(self like&, key: K const&) -> Item like*
    {
        let current = root;
        while(current != btree_invalid_node_id()) {
            let index = lower_bound_in_node(current, key);
            let target = &pool.node(current);
            if(index < (*target).len and keys_equal(key, traits.key(*((*target).items + index)))) {
                return (*target).items + index;
            }
            if((*target).leaf) {
                return nullptr;
            }
            current = pool.child(current, index);
        }
        return nullptr;
    }

    nth_item(self like&, index: usize) -> Item like*
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
                    return (*target).items + slot;
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
        return nullptr;
    }

    rank(self const&, key: K const&) -> usize
    {
        let current = root;
        let result: usize = 0;
        while(current != btree_invalid_node_id()) {
            let target = &pool.node(current);
            let index: usize = 0;
            while(index < (*target).len) {
                let current_key = traits.key(*((*target).items + index));
                if(compare(key, current_key)) {
                    if((*target).leaf) {
                        return result;
                    }
                    current = *((*target).children + index);
                    break;
                }
                if(not (*target).leaf) {
                    result += pool.node(*((*target).children + index)).subtree_size;
                }
                if(compare(current_key, key)) {
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

    insert_unique(self&, value: Item) -> btree_insert_result<Item>
    {
        if(root == btree_invalid_node_id()) {
            root = pool.allocate(true);
            pool.insert_item(root, 0, move value);
            len = 1;
            recompute_node_size(root);
            return btree_insert_result<Item>{ .item = &pool.item(root, 0), .inserted = true };
        }

        if(pool.node(root).len == btree_node_capacity()) {
            let old_root = root;
            root = pool.allocate(false);
            pool.set_child(root, 0, old_root);
            split_child(root, 0);
        }

        return insert_non_full(root, move value);
    }

    insert_non_full(self&, id: u32, value: Item) -> btree_insert_result<Item>
    {
        let target = &pool.node(id);
        let index = lower_bound_in_node(id, traits.key(value));
        if(index < (*target).len and keys_equal(traits.key(value), traits.key(*((*target).items + index)))) {
            return btree_insert_result<Item>{ .item = (*target).items + index, .inserted = false };
        }

        if((*target).leaf) {
            pool.insert_item(id, index, move value);
            len += 1;
            recompute_node_size(id);
            return btree_insert_result<Item>{ .item = &pool.item(id, index), .inserted = true };
        }

        let child_index = index;
        let child_id = pool.child(id, child_index);
        if(pool.node(child_id).len == btree_node_capacity()) {
            split_child(id, child_index);
            let promoted_key = traits.key(*((*target).items + child_index));
            if(compare(traits.key(value), promoted_key)) {
                child_id = pool.child(id, child_index);
            } else if(compare(promoted_key, traits.key(value))) {
                child_id = pool.child(id, child_index + 1);
            } else {
                return btree_insert_result<Item>{ .item = (*target).items + child_index, .inserted = false };
            }
        }

        let result = insert_non_full(child_id, move value);
        if(result.inserted) {
            recompute_node_size(id);
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
            insert_unique(other.pool.item(id, index));
            index += 1;
        }
        if(not (*target).leaf) {
            clone_node_items(other, other.pool.child(id, (*target).len));
        }
    }

    keys_equal(self const&, left: K const&, right: K const&) -> bool
    {
        return not compare(left, right) and not compare(right, left);
    }

    lower_bound_in_node(self const&, id: u32, key: K const&) -> usize
    {
        let target = &pool.node(id);
        let index: usize = 0;
        while(index < (*target).len and compare(traits.key(*((*target).items + index)), key)) {
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

    split_child(self&, parent_id: u32, child_index: usize) -> void
    {
        let child_id = pool.child(parent_id, child_index);
        let child = &pool.node(child_id);
        assert((*child).len == btree_node_capacity(), "btree split_child requires a full child");

        let sibling_id = pool.allocate((*child).leaf);
        let sibling = &pool.node(sibling_id);
        let median = btree_min_items();
        let right_count = btree_min_items();

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
            construct_at((*sibling).items + index, move *((*child).items + median + 1 + index));
            destroy_at((*child).items + median + 1 + index);
            index += 1;
        }
        (*sibling).len = right_count;

        let median_item = move *((*child).items + median);
        destroy_at((*child).items + median);
        (*child).len = median;

        pool.insert_child(parent_id, child_index + 1, sibling_id);
        pool.insert_item(parent_id, child_index, move median_item);

        recompute_node_size(child_id);
        recompute_node_size(sibling_id);
        recompute_node_size(parent_id);
    }

    remove_from_node(self&, id: u32, key: K const&) -> bool
    {
        let target = &pool.node(id);
        let index = lower_bound_in_node(id, key);
        if(index < (*target).len and keys_equal(key, traits.key(pool.item(id, index)))) {
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

    remove_min(self&, id: u32) -> Item
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

    remove_max(self&, id: u32) -> Item
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

    borrow_from_left(self&, parent_id: u32, index: usize) -> void
    {
        let child_id = pool.child(parent_id, index);
        let left_id = pool.child(parent_id, index - 1);
        let parent = &pool.node(parent_id);
        let child = &pool.node(child_id);
        let left = &pool.node(left_id);

        let parent_item = (*parent).items + index - 1;
        let down = move *parent_item;
        destroy_at(parent_item);

        if(not (*child).leaf) {
            let moved_child = pool.remove_child(left_id, (*left).len);
            pool.insert_child(child_id, 0, moved_child);
        }

        let up = pool.remove_item(left_id, (*left).len - 1);
        pool.insert_item(child_id, 0, move down);
        construct_at(parent_item, move up);

        recompute_node_size(left_id);
        recompute_node_size(child_id);
        recompute_node_size(parent_id);
    }

    borrow_from_right(self&, parent_id: u32, index: usize) -> void
    {
        let child_id = pool.child(parent_id, index);
        let right_id = pool.child(parent_id, index + 1);
        let parent = &pool.node(parent_id);
        let child = &pool.node(child_id);
        let right = &pool.node(right_id);

        let parent_item = (*parent).items + index;
        let down = move *parent_item;
        destroy_at(parent_item);

        if(not (*child).leaf) {
            let moved_child = pool.remove_child(right_id, 0);
            pool.set_child(child_id, (*child).len + 1, moved_child);
        }

        let up = pool.remove_item(right_id, 0);
        pool.append_item(child_id, move down);
        construct_at(parent_item, move up);

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
        pool.append_item(left_id, move separator);

        while(pool.node(right_id).len > 0) {
            let item = pool.remove_item(right_id, 0);
            pool.append_item(left_id, move item);
        }

        pool.release(right_id);
        recompute_node_size(left_id);
        recompute_node_size(parent_id);
    }
}
