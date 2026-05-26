export module std.collections.detail.btree_storage;

import std.collections.vector;

export btree_invalid_node_id() -> u32
{
    return 0;
}

export btree_node_capacity() -> usize
{
    return 39;
}

export btree_min_items() -> usize
{
    return 19;
}

btree_chunk_node_count() -> usize
{
    return 256;
}

export struct btree_empty {
}

export struct btree_item_ref<K, V> {
    key: K*;
    value: V*;
}

export struct btree_removed_item<K, V> {
    key: K;
    value: V;
}

export struct btree_node<K, V> {
    leaf: bool;
    len: usize;
    subtree_size: usize;
    keys: storage [K; 39];
    values: storage [V; 39];
    children: u32*;
    next_free: u32;
}

export struct btree_node_chunk<K, V> {
    nodes: btree_node<K, V>*;
    children: u32*;
}

export struct btree_node_pool<K, V> {
    chunks: vector<btree_node_chunk<K, V>>;
    allocated: u32;
    free_head: u32;
}

impl btree_node_pool<K, V> {
    btree_node_pool() = default;

    btree_node_pool(other: this const&) = delete;
    operator =(self&, rhs: this const&) = delete;
    operator =(self&, rhs: this move&) = delete;

    btree_node_pool(other: this move&)
    {
        let result = btree_node_pool<K, V>{
            .chunks = move other.chunks,
            .allocated = other.allocated,
            .free_head = other.free_head
        };
        other.allocated = 0;
        other.free_head = btree_invalid_node_id();
        return result;
    }

    ~btree_node_pool()
    {
        destroy_allocated_nodes();
        destroy_chunks();
    }

    node(self like&, id: u32) -> btree_node<K, V> like&
    {
        assert(id != btree_invalid_node_id(), "btree node id is invalid");
        assert(id <= allocated, "btree node id out of range");
        let raw_index = (id - 1) as usize;
        let chunk_index = raw_index / btree_chunk_node_count();
        let node_index = raw_index % btree_chunk_node_count();
        return ref *(chunks[chunk_index].nodes + node_index);
    }

    key_slot(self like&, id: u32, index: usize) -> K like*
    {
        let target = &node(id);
        assert(index < (*target).len, "btree key index out of bounds");
        return (*target).keys.slot(index);
    }

    value_slot(self like&, id: u32, index: usize) -> V like*
    {
        let target = &node(id);
        assert(index < (*target).len, "btree value index out of bounds");
        return (*target).values.slot(index);
    }

    key(self like&, id: u32, index: usize) -> K like&
    {
        return ref *key_slot(id, index);
    }

    value(self like&, id: u32, index: usize) -> V like&
    {
        return ref *value_slot(id, index);
    }

    item(self&, id: u32, index: usize) -> btree_item_ref<K, V>
    {
        return btree_item_ref<K, V>{
            .key = &key(id, index),
            .value = &value(id, index)
        };
    }

    child(self const&, id: u32, index: usize) -> u32
    {
        let target = &node(id);
        assert(not (*target).leaf, "btree leaf node has no children");
        assert(index <= (*target).len, "btree child index out of bounds");
        return *((*target).children + index);
    }

    set_child(self&, id: u32, index: usize, child_id: u32) -> void
    {
        let target = &node(id);
        assert(not (*target).leaf, "btree leaf node has no children");
        assert(index <= btree_node_capacity(), "btree child set index out of bounds");
        *((*target).children + index) = child_id;
    }

    allocate(self&, leaf: bool) -> u32
    {
        if(free_head != btree_invalid_node_id()) {
            let id = free_head;
            let target = &node(id);
            free_head = (*target).next_free;
            initialize_node(id, leaf);
            return id;
        }

        if((allocated as usize) % btree_chunk_node_count() == 0) {
            chunks.push_back(btree_node_chunk<K, V>{
                .nodes = alloc<btree_node<K, V>>(btree_chunk_node_count()),
                .children = alloc<u32>(btree_chunk_node_count() * (btree_node_capacity() + 1))
            });
        }

        allocated += 1;
        let target = &node(allocated);
        construct_at(target, btree_node<K, V>{});
        initialize_node(allocated, leaf);
        return allocated;
    }

    release(self&, id: u32) -> void
    {
        if(id == btree_invalid_node_id()) {
            return;
        }

        let target = &node(id);
        destroy_node_storage(target);
        (*target).leaf = true;
        (*target).len = 0;
        (*target).subtree_size = 0;
        (*target).children = nullptr;
        (*target).next_free = free_head;
        free_head = id;
    }

    construct_item(self&, id: u32, index: usize, key: K, value: V) -> void
    {
        let target = &node(id);
        construct_at((*target).keys.slot(index), move key);
        construct_at((*target).values.slot(index), move value);
    }

    destroy_item(self&, id: u32, index: usize) -> void
    {
        let target = &node(id);
        destroy_at((*target).keys.slot(index));
        destroy_at((*target).values.slot(index));
    }

    move_item(self&, target_id: u32, target_index: usize, source_id: u32, source_index: usize) -> void
    {
        let target = &node(target_id);
        let source = &node(source_id);
        construct_at((*target).keys.slot(target_index), move *((*source).keys.slot(source_index)));
        construct_at((*target).values.slot(target_index), move *((*source).values.slot(source_index)));
        destroy_item(source_id, source_index);
    }

    shift_items_right(self&, id: u32, index: usize, count: usize) -> void
    {
        if(count == 0) {
            return;
        }

        let target = &node(id);
        assert((*target).len + count <= btree_node_capacity(), "btree shift_items_right over capacity");
        assert(index <= (*target).len, "btree shift_items_right index out of bounds");

        let current = (*target).len;
        while(current > index) {
            let source = current - 1;
            move_item(id, source + count, id, source);
            current -= 1;
        }
        (*target).len += count;
    }

    shift_destroyed_prefix_left(self&, id: u32, count: usize) -> void
    {
        if(count == 0) {
            return;
        }

        let target = &node(id);
        assert(count <= (*target).len, "btree shift_destroyed_prefix_left count out of bounds");

        let old_len = (*target).len;
        let current: usize = 0;
        while(current + count < old_len) {
            move_item(id, current, id, current + count);
            current += 1;
        }
        (*target).len -= count;
    }

    shrink_destroyed_suffix(self&, id: u32, count: usize) -> void
    {
        if(count == 0) {
            return;
        }

        let target = &node(id);
        assert(count <= (*target).len, "btree shrink_destroyed_suffix count out of bounds");
        (*target).len -= count;
    }

    insert_item(self&, id: u32, index: usize, key: K, value: V) -> void
    {
        let target = &node(id);
        assert((*target).len < btree_node_capacity(), "btree insert_item on full node");
        assert(index <= (*target).len, "btree insert_item index out of bounds");

        shift_items_right(id, index, 1);
        construct_item(id, index, move key, move value);
    }

    append_item(self&, id: u32, key: K, value: V) -> void
    {
        let target = &node(id);
        insert_item(id, (*target).len, move key, move value);
    }

    remove_item(self&, id: u32, index: usize) -> btree_removed_item<K, V>
    {
        let target = &node(id);
        assert(index < (*target).len, "btree remove_item index out of bounds");

        let result = take_item(id, index);

        if(index + 1 < (*target).len) {
            let old_len = (*target).len;
            let current = index;
            while(current + 1 < old_len) {
                move_item(id, current, id, current + 1);
                current += 1;
            }
        }
        (*target).len -= 1;
        return move result;
    }

    take_item(self&, id: u32, index: usize) -> btree_removed_item<K, V>
    {
        let target = &node(id);
        assert(index < (*target).len, "btree take_item index out of bounds");

        let result = btree_removed_item<K, V>{
            .key = move *((*target).keys.slot(index)),
            .value = move *((*target).values.slot(index))
        };
        destroy_item(id, index);
        return move result;
    }

    replace_item(self&, id: u32, index: usize, item: btree_removed_item<K, V>) -> void
    {
        let target = &node(id);
        assert(index < (*target).len, "btree replace_item index out of bounds");
        destroy_item(id, index);
        construct_item(id, index, move item.key, move item.value);
    }

    insert_child(self&, id: u32, index: usize, child_id: u32) -> void
    {
        let target = &node(id);
        assert(not (*target).leaf, "btree leaf node has no children");
        assert(index <= (*target).len + 1, "btree insert_child index out of bounds");
        assert((*target).len < btree_node_capacity(), "btree insert_child on full node");

        let current = (*target).len + 1;
        while(current > index) {
            *((*target).children + current) = *((*target).children + current - 1);
            current -= 1;
        }
        *((*target).children + index) = child_id;
    }

    remove_child(self&, id: u32, index: usize) -> u32
    {
        let target = &node(id);
        assert(not (*target).leaf, "btree leaf node has no children");
        assert(index <= (*target).len, "btree remove_child index out of bounds");

        let result = *((*target).children + index);
        let current = index;
        while(current < (*target).len) {
            *((*target).children + current) = *((*target).children + current + 1);
            current += 1;
        }
        *((*target).children + (*target).len) = btree_invalid_node_id();
        return result;
    }

    clear(self&) -> void
    {
        destroy_allocated_nodes();
        destroy_chunks();
        chunks.clear();
        allocated = 0;
        free_head = btree_invalid_node_id();
    }

    initialize_node(self&, id: u32, leaf: bool) -> void
    {
        let target = &node(id);
        let raw_index = (id - 1) as usize;
        let chunk_index = raw_index / btree_chunk_node_count();
        let node_index = raw_index % btree_chunk_node_count();

        (*target).leaf = leaf;
        (*target).len = 0;
        (*target).subtree_size = 0;
        (*target).children = nullptr;
        (*target).next_free = btree_invalid_node_id();

        if(not leaf) {
            (*target).children = chunks[chunk_index].children + node_index * (btree_node_capacity() + 1);
            let index: usize = 0;
            while(index <= btree_node_capacity()) {
                construct_at((*target).children + index, btree_invalid_node_id());
                index += 1;
            }
        }
    }

    destroy_node_storage(self&, target: btree_node<K, V>*) -> void
    {
        let index: usize = 0;
        while(index < (*target).len) {
            destroy_at((*target).keys.slot(index));
            destroy_at((*target).values.slot(index));
            index += 1;
        }

        if((*target).children != nullptr) {
            let child_index: usize = 0;
            while(child_index <= btree_node_capacity()) {
                destroy_at((*target).children + child_index);
                child_index += 1;
            }
        }
    }

    destroy_allocated_nodes(self&) -> void
    {
        let id: u32 = 1;
        while(id <= allocated) {
            let target = &node(id);
            destroy_node_storage(target);
            (*target).children = nullptr;
            (*target).len = 0;
            (*target).subtree_size = 0;
            destroy_at(target);
            id += 1;
        }
    }

    destroy_chunks(self&) -> void
    {
        let index: usize = 0;
        while(index < chunks.size()) {
            let chunk = &chunks[index];
            if((*chunk).nodes != nullptr) {
                free((*chunk).nodes);
            }
            if((*chunk).children != nullptr) {
                free((*chunk).children);
            }
            index += 1;
        }
    }
}
