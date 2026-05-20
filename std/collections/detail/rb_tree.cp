export module std.collections.detail.rb_tree;

import std.compare;

enum rb_color : u8 {
    red = 0;
    black = 1;
}

export struct rb_node<Node> {
    parent: rb_node<Node>*;
    left: rb_node<Node>*;
    right: rb_node<Node>*;
    color: rb_color;
    size: usize;
    value: Node;
}

export struct rb_insert_result<Node> {
    node: rb_node<Node>*;
    inserted: bool;
}

node_size<Node>(node: rb_node<Node> const*) -> usize
{
    if(node == nullptr) {
        return 0;
    }
    return (*node).size;
}

is_red<Node>(node: rb_node<Node> const*) -> bool
{
    return node != nullptr and (*node).color == rb_color::red;
}

is_black<Node>(node: rb_node<Node> const*) -> bool
{
    return node == nullptr or (*node).color == rb_color::black;
}

set_black<Node>(node: rb_node<Node>*) -> void
{
    if(node != nullptr) {
        (*node).color = rb_color::black;
    }
}

set_red<Node>(node: rb_node<Node>*) -> void
{
    if(node != nullptr) {
        (*node).color = rb_color::red;
    }
}

update_size<Node>(node: rb_node<Node>*) -> void
{
    if(node != nullptr) {
        (*node).size = node_size((*node).left) + node_size((*node).right) + 1;
    }
}

export struct rb_tree<K, Node, Compare: strict_weak_order<K> = less<K>> {
    root: rb_node<Node>*;
    len: usize;
    compare: Compare;
}

impl rb_tree<K, Node, Compare> {
    rb_tree()
    {
        return rb_tree<K, Node, Compare>{
            .root = nullptr,
            .len = 0,
            .compare = Compare{}
        };
    }

    rb_tree(other: this const&)
    {
        return rb_tree<K, Node, Compare>{
            .root = clone_subtree(other.root, nullptr),
            .len = other.len,
            .compare = other.compare
        };
    }

    rb_tree(other: this move&)
    {
        let result = rb_tree<K, Node, Compare>{
            .root = other.root,
            .len = other.len,
            .compare = move other.compare
        };
        other.root = nullptr;
        other.len = 0;
        return result;
    }

    ~rb_tree()
    {
        clear();
    }

    operator =(self&, rhs: this const&) -> this&
    {
        if(&rhs == &self) {
            return ref self;
        }

        clear();
        root = clone_subtree(rhs.root, nullptr);
        len = rhs.len;
        compare = rhs.compare;
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
        compare = move rhs.compare;
        rhs.root = nullptr;
        rhs.len = 0;
        return ref self;
    }

    clear(self&) -> void
    {
        delete_subtree(root);
        root = nullptr;
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

    find_node(self like&, key: K const&) -> rb_node<Node> like*
    {
        let current = root;
        while(current != nullptr) {
            if(compare(key, (*current).value.key)) {
                current = (*current).left;
            } else if(compare((*current).value.key, key)) {
                current = (*current).right;
            } else {
                return current;
            }
        }
        return nullptr;
    }

    nth_node(self like&, index: usize) -> rb_node<Node> like*
    {
        let current = root;
        let remaining = index;
        while(current != nullptr) {
            let left_size = node_size((*current).left);
            if(remaining < left_size) {
                current = (*current).left;
            } else if(remaining == left_size) {
                return current;
            } else {
                remaining -= left_size + 1;
                current = (*current).right;
            }
        }
        return nullptr;
    }

    rank(self const&, key: K const&) -> usize
    {
        let current = root;
        let result: usize = 0;
        while(current != nullptr) {
            if(compare(key, (*current).value.key)) {
                current = (*current).left;
            } else if(compare((*current).value.key, key)) {
                result += node_size((*current).left) + 1;
                current = (*current).right;
            } else {
                return result + node_size((*current).left);
            }
        }
        return result;
    }

    insert_unique(self&, value: Node) -> rb_insert_result<Node>
    {
        let parent: rb_node<Node>* = nullptr;
        let current = root;
        while(current != nullptr) {
            parent = current;
            if(compare(value.key, (*current).value.key)) {
                current = (*current).left;
            } else if(compare((*current).value.key, value.key)) {
                current = (*current).right;
            } else {
                return rb_insert_result<Node>{ .node = current, .inserted = false };
            }
        }

        let node = new rb_node<Node>{
            .parent = parent,
            .left = nullptr,
            .right = nullptr,
            .color = rb_color::red,
            .size = 1,
            .value = move value
        };
        if(parent == nullptr) {
            root = node;
        } else if(compare((*node).value.key, (*parent).value.key)) {
            (*parent).left = node;
        } else {
            (*parent).right = node;
        }
        len += 1;
        recompute_sizes_upward(parent);
        fix_insert(node);
        return rb_insert_result<Node>{ .node = node, .inserted = true };
    }

    remove(self&, key: K const&) -> bool
    {
        let node = find_node(key);
        if(node == nullptr) {
            return false;
        }

        erase_node(node);
        len -= 1;
        return true;
    }

    clone_subtree(self const&, source: rb_node<Node> const*, parent: rb_node<Node>*) -> rb_node<Node>*
    {
        if(source == nullptr) {
            return nullptr;
        }

        let copy = new rb_node<Node>{
            .parent = parent,
            .left = nullptr,
            .right = nullptr,
            .color = (*source).color,
            .size = (*source).size,
            .value = (*source).value
        };
        (*copy).left = clone_subtree((*source).left, copy);
        (*copy).right = clone_subtree((*source).right, copy);
        return copy;
    }

    delete_subtree(self&, node: rb_node<Node>*) -> void
    {
        if(node == nullptr) {
            return;
        }

        delete_subtree((*node).left);
        delete_subtree((*node).right);
        delete node;
    }

    recompute_sizes_upward(self&, node: rb_node<Node>*) -> void
    {
        let current = node;
        while(current != nullptr) {
            update_size(current);
            current = (*current).parent;
        }
    }

    minimum_node(self like&, node: rb_node<Node> like*) -> rb_node<Node> like*
    {
        let current = node;
        while(current != nullptr and (*current).left != nullptr) {
            current = (*current).left;
        }
        return current;
    }

    rotate_left(self&, node: rb_node<Node>*) -> void
    {
        let pivot = (*node).right;
        assert(pivot != nullptr, "rb_tree rotate_left requires right child");

        (*node).right = (*pivot).left;
        if((*pivot).left != nullptr) {
            (*(*pivot).left).parent = node;
        }

        (*pivot).parent = (*node).parent;
        if((*node).parent == nullptr) {
            root = pivot;
        } else if(node == (*(*node).parent).left) {
            (*(*node).parent).left = pivot;
        } else {
            (*(*node).parent).right = pivot;
        }

        (*pivot).left = node;
        (*node).parent = pivot;
        update_size(node);
        update_size(pivot);
        recompute_sizes_upward((*pivot).parent);
    }

    rotate_right(self&, node: rb_node<Node>*) -> void
    {
        let pivot = (*node).left;
        assert(pivot != nullptr, "rb_tree rotate_right requires left child");

        (*node).left = (*pivot).right;
        if((*pivot).right != nullptr) {
            (*(*pivot).right).parent = node;
        }

        (*pivot).parent = (*node).parent;
        if((*node).parent == nullptr) {
            root = pivot;
        } else if(node == (*(*node).parent).right) {
            (*(*node).parent).right = pivot;
        } else {
            (*(*node).parent).left = pivot;
        }

        (*pivot).right = node;
        (*node).parent = pivot;
        update_size(node);
        update_size(pivot);
        recompute_sizes_upward((*pivot).parent);
    }

    fix_insert(self&, node: rb_node<Node>*) -> void
    {
        let current = node;
        while(current != root and is_red((*current).parent)) {
            let parent = (*current).parent;
            let grand = (*parent).parent;
            if(parent == (*grand).left) {
                let uncle = (*grand).right;
                if(is_red(uncle)) {
                    set_black(parent);
                    set_black(uncle);
                    set_red(grand);
                    current = grand;
                } else {
                    if(current == (*parent).right) {
                        current = parent;
                        rotate_left(current);
                    }
                    set_black((*current).parent);
                    set_red((*(*current).parent).parent);
                    rotate_right((*(*current).parent).parent);
                }
            } else {
                let uncle = (*grand).left;
                if(is_red(uncle)) {
                    set_black(parent);
                    set_black(uncle);
                    set_red(grand);
                    current = grand;
                } else {
                    if(current == (*parent).left) {
                        current = parent;
                        rotate_right(current);
                    }
                    set_black((*current).parent);
                    set_red((*(*current).parent).parent);
                    rotate_left((*(*current).parent).parent);
                }
            }
        }
        set_black(root);
    }

    transplant(self&, old_node: rb_node<Node>*, new_node: rb_node<Node>*) -> void
    {
        if((*old_node).parent == nullptr) {
            root = new_node;
        } else if(old_node == (*(*old_node).parent).left) {
            (*(*old_node).parent).left = new_node;
        } else {
            (*(*old_node).parent).right = new_node;
        }

        if(new_node != nullptr) {
            (*new_node).parent = (*old_node).parent;
        }
    }

    erase_node(self&, target: rb_node<Node>*) -> void
    {
        let moved = target;
        let moved_was_red = (*moved).color == rb_color::red;
        let fix_node: rb_node<Node>* = nullptr;
        let fix_parent: rb_node<Node>* = nullptr;

        if((*target).left == nullptr) {
            fix_node = (*target).right;
            fix_parent = (*target).parent;
            transplant(target, (*target).right);
            recompute_sizes_upward(fix_parent);
        } else if((*target).right == nullptr) {
            fix_node = (*target).left;
            fix_parent = (*target).parent;
            transplant(target, (*target).left);
            recompute_sizes_upward(fix_parent);
        } else {
            moved = minimum_node((*target).right);
            moved_was_red = (*moved).color == rb_color::red;
            fix_node = (*moved).right;
            if((*moved).parent == target) {
                fix_parent = moved;
                if(fix_node != nullptr) {
                    (*fix_node).parent = moved;
                }
            } else {
                fix_parent = (*moved).parent;
                transplant(moved, (*moved).right);
                recompute_sizes_upward(fix_parent);
                (*moved).right = (*target).right;
                (*(*moved).right).parent = moved;
            }

            transplant(target, moved);
            (*moved).left = (*target).left;
            (*(*moved).left).parent = moved;
            (*moved).color = (*target).color;
            update_size(moved);
            recompute_sizes_upward((*moved).parent);
        }

        delete target;
        if(not moved_was_red) {
            fix_delete(fix_node, fix_parent);
        }
    }

    fix_delete(self&, node: rb_node<Node>*, parent: rb_node<Node>*) -> void
    {
        let current = node;
        let current_parent = parent;
        while(current != root and is_black(current)) {
            if(current_parent == nullptr) {
                return;
            }

            if(current == (*current_parent).left) {
                let sibling = (*current_parent).right;
                if(is_red(sibling)) {
                    set_black(sibling);
                    set_red(current_parent);
                    rotate_left(current_parent);
                    sibling = (*current_parent).right;
                }

                if(sibling == nullptr) {
                    current = current_parent;
                    current_parent = (*current).parent;
                } else if(is_black((*sibling).left) and is_black((*sibling).right)) {
                    set_red(sibling);
                    current = current_parent;
                    current_parent = (*current).parent;
                } else {
                    if(is_black((*sibling).right)) {
                        set_black((*sibling).left);
                        set_red(sibling);
                        rotate_right(sibling);
                        sibling = (*current_parent).right;
                    }
                    (*sibling).color = (*current_parent).color;
                    set_black(current_parent);
                    set_black((*sibling).right);
                    rotate_left(current_parent);
                    current = root;
                    current_parent = nullptr;
                }
            } else {
                let sibling = (*current_parent).left;
                if(is_red(sibling)) {
                    set_black(sibling);
                    set_red(current_parent);
                    rotate_right(current_parent);
                    sibling = (*current_parent).left;
                }

                if(sibling == nullptr) {
                    current = current_parent;
                    current_parent = (*current).parent;
                } else if(is_black((*sibling).right) and is_black((*sibling).left)) {
                    set_red(sibling);
                    current = current_parent;
                    current_parent = (*current).parent;
                } else {
                    if(is_black((*sibling).left)) {
                        set_black((*sibling).right);
                        set_red(sibling);
                        rotate_left(sibling);
                        sibling = (*current_parent).left;
                    }
                    (*sibling).color = (*current_parent).color;
                    set_black(current_parent);
                    set_black((*sibling).left);
                    rotate_right(current_parent);
                    current = root;
                    current_parent = nullptr;
                }
            }
        }
        set_black(current);
    }
}
