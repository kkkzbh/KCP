export module std.algorithm.sort;

import std.compare;
import std.memory.span;

swap_values<T: mutable_object>(values: span<T>, left: usize, right: usize) -> void
{
    if(left == right) {
        return;
    }

    let item: T = values[left];
    values[left] = values[right];
    values[right] = item;
}

insertion_sort_range<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, last: usize, compare: Compare) -> void
{
    let index = first + 1;
    while(index < last) {
        let item: T = values[index];
        let cursor = index;
        while(cursor > first and compare(item, values[cursor - 1])) {
            values[cursor] = values[cursor - 1];
            cursor -= 1;
        }
        values[cursor] = item;
        index += 1;
    }
}

quick_sort_range<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, last: usize, compare: Compare) -> void
{
    if(last <= first + 1) {
        return;
    }

    if(last - first <= 16) {
        insertion_sort_range(values, first, last, compare);
        return;
    }

    let mid = first + (last - first) / 2;
    let pivot: T = values[mid];
    let less_end = first;
    let index = first;
    let greater_begin = last;
    while(index < greater_begin) {
        if(compare(values[index], pivot)) {
            swap_values(values, less_end, index);
            less_end += 1;
            index += 1;
        } else if(compare(pivot, values[index])) {
            greater_begin -= 1;
            swap_values(values, index, greater_begin);
        } else {
            index += 1;
        }
    }

    quick_sort_range(values, first, less_end, compare);
    quick_sort_range(values, greater_begin, last, compare);
}

export sort<T: mutable_object, Compare: strict_weak_order<T> = less<T>>(values: span<T>, compare: Compare = Compare{}) -> void
{
    quick_sort_range(values, 0 as usize, values.size(), compare);
}
