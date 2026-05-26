export module std.algorithm.sort;

import std.collections.vector;
import std.compare;
import std.memory.raw_buffer;
import std.memory.span;

struct tim_sort_run {
    start: usize;
    len: usize;
}

swap_values<T: mutable_object>(values: span<T>, left: usize, right: usize) -> void
{
    if(left == right) {
        return;
    }

    let item: T = values[left];
    values[left] = values[right];
    values[right] = item;
}

reverse_range<T: mutable_object>(values: span<T>, first: usize, last: usize) -> void
{
    let left = first;
    let right = last;
    while(left < right) {
        right -= 1;
        if(left >= right) {
            return;
        }
        swap_values(values, left, right);
        left += 1;
    }
}

min_usize(left: usize, right: usize) -> usize
{
    if(left < right) {
        return left;
    }
    return right;
}

tim_sort_min_run(count: usize) -> usize
{
    let remaining = count;
    let remainder: usize = 0;
    while(remaining >= 64) {
        if(remaining % 2 != 0) {
            remainder = 1;
        }
        remaining = remaining / 2;
    }
    return remaining + remainder;
}

binary_insertion_sort<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, last: usize, sorted_until: usize, compare: Compare) -> void
{
    let index = sorted_until;
    if(index == first) {
        index += 1;
    }

    while(index < last) {
        let item: T = values[index];
        let left = first;
        let right = index;
        while(left < right) {
            let mid = left + (right - left) / 2;
            if(compare(item, values[mid])) {
                right = mid;
            } else {
                left = mid + 1;
            }
        }

        let cursor = index;
        while(cursor > left) {
            values[cursor] = values[cursor - 1];
            cursor -= 1;
        }
        values[left] = item;
        index += 1;
    }
}

count_run_and_make_ascending<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, last: usize, compare: Compare) -> usize
{
    let current = first + 1;
    if(current == last) {
        return 1;
    }

    current += 1;
    if(compare(values[first + 1], values[first])) {
        while(current < last and compare(values[current], values[current - 1])) {
            current += 1;
        }
        reverse_range(values, first, current);
    } else {
        while(current < last and not compare(values[current], values[current - 1])) {
            current += 1;
        }
    }

    return current - first;
}

merge_low<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, temp: raw_buffer<T>&, left: usize, left_len: usize, right: usize, right_len: usize, compare: Compare) -> void
{
    let index: usize = 0;
    while(index < left_len) {
        construct_at(temp.data() + index, values[left + index]);
        index += 1;
    }

    let temp_index: usize = 0;
    let right_index = right;
    let write_index = left;
    while(temp_index < left_len and right_index < right + right_len) {
        if(compare(values[right_index], *(temp.data() + temp_index))) {
            values[write_index] = values[right_index];
            right_index += 1;
        } else {
            values[write_index] = *(temp.data() + temp_index);
            temp_index += 1;
        }
        write_index += 1;
    }

    while(temp_index < left_len) {
        values[write_index] = *(temp.data() + temp_index);
        temp_index += 1;
        write_index += 1;
    }

    index = 0;
    while(index < left_len) {
        destroy_at(temp.data() + index);
        index += 1;
    }
}

merge_high<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, temp: raw_buffer<T>&, left: usize, left_len: usize, right: usize, right_len: usize, compare: Compare) -> void
{
    let index: usize = 0;
    while(index < right_len) {
        construct_at(temp.data() + index, values[right + index]);
        index += 1;
    }

    let left_remaining = left_len;
    let temp_remaining = right_len;
    let write_index = left + left_len + right_len;
    while(left_remaining > 0 and temp_remaining > 0) {
        let left_index = left + left_remaining - 1;
        let temp_index = temp_remaining - 1;
        write_index -= 1;
        if(compare(*(temp.data() + temp_index), values[left_index])) {
            values[write_index] = values[left_index];
            left_remaining -= 1;
        } else {
            values[write_index] = *(temp.data() + temp_index);
            temp_remaining -= 1;
        }
    }

    while(temp_remaining > 0) {
        temp_remaining -= 1;
        write_index -= 1;
        values[write_index] = *(temp.data() + temp_remaining);
    }

    index = 0;
    while(index < right_len) {
        destroy_at(temp.data() + index);
        index += 1;
    }
}

tim_sort_merge<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, temp: raw_buffer<T>&, left: usize, left_len: usize, right: usize, right_len: usize, compare: Compare) -> void
{
    if(left_len <= right_len) {
        merge_low(values, temp, left, left_len, right, right_len, compare);
    } else {
        merge_high(values, temp, left, left_len, right, right_len, compare);
    }
}

tim_sort_merge_at<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, temp: raw_buffer<T>&, runs: vector<tim_sort_run>&, run_index: usize, compare: Compare) -> void
{
    let left = runs[run_index].start;
    let left_len = runs[run_index].len;
    let right = runs[run_index + 1].start;
    let right_len = runs[run_index + 1].len;
    runs[run_index].len = left_len + right_len;

    let index = run_index + 1;
    while(index + 1 < runs.size()) {
        runs[index] = runs[index + 1];
        index += 1;
    }
    runs.pop_back();

    if(not compare(values[right], values[right - 1])) {
        return;
    }
    tim_sort_merge(values, temp, left, left_len, right, right_len, compare);
}

tim_sort_collapse<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, temp: raw_buffer<T>&, runs: vector<tim_sort_run>&, compare: Compare) -> void
{
    while(runs.size() > 1) {
        let current = runs.size() - 2;
        if(current > 0 and runs[current - 1].len <= runs[current].len + runs[current + 1].len) {
            let merge_index = current;
            if(runs[current - 1].len < runs[current + 1].len) {
                merge_index = current - 1;
            }
            tim_sort_merge_at(values, temp, runs, merge_index, compare);
        } else if(runs[current].len <= runs[current + 1].len) {
            tim_sort_merge_at(values, temp, runs, current, compare);
        } else {
            return;
        }
    }
}

tim_sort_force_collapse<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, temp: raw_buffer<T>&, runs: vector<tim_sort_run>&, compare: Compare) -> void
{
    while(runs.size() > 1) {
        let current = runs.size() - 2;
        if(current > 0 and runs[current - 1].len < runs[current + 1].len) {
            tim_sort_merge_at(values, temp, runs, current - 1, compare);
        } else {
            tim_sort_merge_at(values, temp, runs, current, compare);
        }
    }
}

timsort_range<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, compare: Compare) -> void
{
    let count = values.size();
    if(count < 2) {
        return;
    }

    if(count < 64) {
        binary_insertion_sort(values, 0 as usize, count, 1 as usize, compare);
        return;
    }

    let min_run = tim_sort_min_run(count);
    let temp = raw_buffer<T>{count};
    let runs = vector<tim_sort_run>{};
    let current: usize = 0;
    let remaining = count;
    while(remaining > 0) {
        let run_len = count_run_and_make_ascending(values, current, count, compare);
        if(run_len < min_run) {
            let forced_len = min_usize(min_run, remaining);
            binary_insertion_sort(values, current, current + forced_len, current + run_len, compare);
            run_len = forced_len;
        }

        runs.push_back(tim_sort_run{ .start = current, .len = run_len });
        tim_sort_collapse(values, temp, runs, compare);
        current += run_len;
        remaining -= run_len;
    }

    tim_sort_force_collapse(values, temp, runs, compare);
}

sort_span<T: mutable_object, Compare: strict_weak_order<T> = less<T>>(values: span<T>, compare: Compare = Compare{}) -> void
{
    timsort_range(values, compare);
}

export sort<R: contiguous_mutable_range, Compare: strict_weak_order<R::item> = less<R::item>>(values: R forward&, compare: Compare = Compare{}) -> void
{
    sort_span(span<R::item>{values.data(), values.size()}, compare);
}
