export module std.collections.detail.btree_storage;

import std.collections.vector;

export btree_invalid_node_id() -> u32
{
    return 0;
}

export btree_node_capacity() -> usize
{
    return 15;
}

export btree_min_items() -> usize
{
    return 7;
}

btree_chunk_node_count() -> usize
{
    return 256;
}

export struct btree_node<Item> {
    leaf: bool;
    len: usize;
    subtree_size: usize;
    items: Item*;
    children: u32*;
    next_free: u32;
}

export struct btree_node_chunk<Item> {
    nodes: btree_node<Item>*;
    items: Item*;
    children: u32*;
}

export struct btree_node_pool<Item> {
    chunks: vector<btree_node_chunk<Item>*>;
    allocated: u32;
    free_head: u32;
}

impl btree_node_pool<Item> {
    btree_node_pool() = default;

    btree_node_pool(other: this const&) = delete;
    operator =(self&, rhs: this const&) = delete;
    operator =(self&, rhs: this move&) = delete;

    btree_node_pool(other: this move&)
    {
        let result = btree_node_pool<Item>{
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

    node(self like&, id: u32) -> btree_node<Item> like&
    {
        assert(id != btree_invalid_node_id(), "btree node id is invalid");
        assert(id <= allocated, "btree node id out of range");
        let raw_index = (id - 1) as usize;
        let chunk_index = raw_index / btree_chunk_node_count();
        let node_index = raw_index % btree_chunk_node_count();
        return ref *((*chunks[chunk_index]).nodes + node_index);
    }

    item(self like&, id: u32, index: usize) -> Item like&
    {
        let target = &node(id);
        assert(index < (*target).len, "btree item index out of bounds");
        return ref *((*target).items + index);
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
            let chunk = alloc<btree_node_chunk<Item>>(1);
            construct_at(chunk, btree_node_chunk<Item>{
                .nodes = alloc<btree_node<Item>>(btree_chunk_node_count()),
                .items = alloc<Item>(btree_chunk_node_count() * btree_node_capacity()),
                .children = alloc<u32>(btree_chunk_node_count() * (btree_node_capacity() + 1))
            });
            chunks.push_back(chunk);
        }

        allocated += 1;
        let target = &node(allocated);
        construct_at(target, btree_node<Item>{});
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
        (*target).items = nullptr;
        (*target).children = nullptr;
        (*target).next_free = free_head;
        free_head = id;
    }

    insert_item(self&, id: u32, index: usize, value: Item) -> void
    {
        let target = &node(id);
        assert((*target).len < btree_node_capacity(), "btree insert_item on full node");
        assert(index <= (*target).len, "btree insert_item index out of bounds");

        let current = (*target).len;
        while(current > index) {
            construct_at((*target).items + current, move *((*target).items + current - 1));
            destroy_at((*target).items + current - 1);
            current -= 1;
        }
        construct_at((*target).items + index, move value);
        (*target).len += 1;
    }

    append_item(self&, id: u32, value: Item) -> void
    {
        let target = &node(id);
        insert_item(id, (*target).len, move value);
    }

    remove_item(self&, id: u32, index: usize) -> Item
    {
        let target = &node(id);
        assert(index < (*target).len, "btree remove_item index out of bounds");

        let result = move *((*target).items + index);
        destroy_at((*target).items + index);
        let current = index;
        while(current + 1 < (*target).len) {
            construct_at((*target).items + current, move *((*target).items + current + 1));
            destroy_at((*target).items + current + 1);
            current += 1;
        }
        (*target).len -= 1;
        return move result;
    }

    replace_item(self&, id: u32, index: usize, value: Item) -> void
    {
        let target = &node(id);
        assert(index < (*target).len, "btree replace_item index out of bounds");
        destroy_at((*target).items + index);
        construct_at((*target).items + index, move value);
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
        (*target).items = (*chunks[chunk_index]).items + node_index * btree_node_capacity();
        (*target).children = nullptr;
        (*target).next_free = btree_invalid_node_id();

        if(not leaf) {
            (*target).children = (*chunks[chunk_index]).children + node_index * (btree_node_capacity() + 1);
            let index: usize = 0;
            while(index <= btree_node_capacity()) {
                construct_at((*target).children + index, btree_invalid_node_id());
                index += 1;
            }
        }
    }

    destroy_node_storage(self&, target: btree_node<Item>*) -> void
    {
        if((*target).items != nullptr) {
            let index: usize = 0;
            while(index < (*target).len) {
                destroy_at((*target).items + index);
                index += 1;
            }
        }

        if((*target).children != nullptr) {
            let index: usize = 0;
            while(index <= btree_node_capacity()) {
                destroy_at((*target).children + index);
                index += 1;
            }
        }
    }

    destroy_allocated_nodes(self&) -> void
    {
        let id: u32 = 1;
        while(id <= allocated) {
            let target = &node(id);
            destroy_node_storage(target);
            (*target).items = nullptr;
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
            let chunk = chunks[index];
            if((*chunk).nodes != nullptr) {
                free((*chunk).nodes);
            }
            if((*chunk).items != nullptr) {
                free((*chunk).items);
            }
            if((*chunk).children != nullptr) {
                free((*chunk).children);
            }
            destroy_at(chunk);
            free(chunk);
            index += 1;
        }
    }
}
