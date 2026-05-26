export module std.algorithm.sort;

import std.collections.vector;
import std.compare;
import std.memory.raw_buffer;
import std.memory.span;

struct power_sort_run {
    start: usize;
    len: usize;
    power: usize;
}

struct unstable_partition {
    less_end: usize;
    greater_start: usize;
}

swap_values<T: mutable_object>(values: span<T>, left: usize, right: usize) -> void
{
    if(left == right) {
        return;
    }

    let data = values.data();
    let item: T = move *(data + left);
    *(data + left) = move *(data + right);
    *(data + right) = move item;
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

floor_log2(value: usize) -> usize
{
    let current = value;
    let result: usize = 0;
    while(current > 1) {
        current = current / 2;
        result += 1;
    }
    return result;
}

stable_sort_min_run(count: usize) -> usize
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

finish_if_monotonic<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, compare: Compare) -> bool
{
    let count = values.size();
    if(count < 2) {
        return true;
    }

    let data = values.data();
    let index: usize = 1;
    while(index < count and not compare(*(data + index), *(data + index - 1))) {
        index += 1;
    }
    if(index == count) {
        return true;
    }

    index = 1;
    while(index < count and compare(*(data + index), *(data + index - 1))) {
        index += 1;
    }
    if(index == count) {
        reverse_range(values, 0 as usize, count);
        return true;
    }

    return false;
}

power_sort_node_power(count: usize, left: power_sort_run, right: power_sort_run) -> usize
{
    let left_mid = left.start * 2 + left.len;
    let right_mid = right.start * 2 + right.len;
    let denominator = count * 2;
    let power: usize = 0;
    let scale: usize = 1;
    while(true) {
        power += 1;
        scale *= 2;
        if((left_mid * scale) / denominator != (right_mid * scale) / denominator) {
            return power;
        }
    }
    unreachable();
}

binary_insertion_sort<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, last: usize, sorted_until: usize, compare: Compare) -> void
{
    let data = values.data();
    let index = sorted_until;
    if(index == first) {
        index += 1;
    }

    while(index < last) {
        let item: T = move *(data + index);
        let left = first;
        let right = index;
        while(left < right) {
            let mid = left + (right - left) / 2;
            if(compare(item, *(data + mid))) {
                right = mid;
            } else {
                left = mid + 1;
            }
        }

        let cursor = index;
        while(cursor > left) {
            *(data + cursor) = move *(data + cursor - 1);
            cursor -= 1;
        }
        *(data + left) = move item;
        index += 1;
    }
}

median_index<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, middle: usize, last: usize, compare: Compare) -> usize
{
    let data = values.data();
    if(compare(*(data + middle), *(data + first))) {
        if(compare(*(data + last), *(data + middle))) {
            return middle;
        }
        if(compare(*(data + last), *(data + first))) {
            return last;
        }
        return first;
    }
    if(compare(*(data + last), *(data + first))) {
        return first;
    }
    if(compare(*(data + last), *(data + middle))) {
        return last;
    }
    return middle;
}

equivalent_values<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, left: usize, right: usize, compare: Compare) -> bool
{
    let data = values.data();
    return not compare(*(data + left), *(data + right)) and not compare(*(data + right), *(data + left));
}

sample_has_duplicates<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, last: usize, compare: Compare) -> bool
{
    let count = last - first;
    let middle = first + count / 2;
    if(equivalent_values(values, first, middle, compare) or equivalent_values(values, middle, last - 1, compare) or equivalent_values(values, first, last - 1, compare)) {
        return true;
    }
    if(count < 128) {
        return false;
    }

    let step = count / 8;
    let a = first + step;
    let b = first + step * 2;
    let c = middle - step;
    let d = middle + step;
    let e = last - 1 - step * 2;
    let f = last - 1 - step;
    return equivalent_values(values, first, a, compare)
        or equivalent_values(values, first, b, compare)
        or equivalent_values(values, a, b, compare)
        or equivalent_values(values, c, middle, compare)
        or equivalent_values(values, c, d, compare)
        or equivalent_values(values, middle, d, compare)
        or equivalent_values(values, e, f, compare)
        or equivalent_values(values, e, last - 1, compare)
        or equivalent_values(values, f, last - 1, compare);
}

recursive_pivot_sample<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, middle: usize, last: usize, count: usize, compare: Compare) -> usize
{
    let left = first;
    let center = middle;
    let right = last;
    if(count * 8 >= 64) {
        let step = count / 8;
        left = recursive_pivot_sample(values, left, left + step * 4, left + step * 7, step, compare);
        center = recursive_pivot_sample(values, center, center + step * 4, center + step * 7, step, compare);
        right = recursive_pivot_sample(values, right, right + step * 4, right + step * 7, step, compare);
    }

    return median_index(values, left, center, right, compare);
}

choose_unstable_pivot<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, last: usize, compare: Compare) -> usize
{
    let count = last - first;
    let middle = first + count / 2;
    if(count < 128) {
        return median_index(values, first, middle, last - 1, compare);
    }

    let step = count / 8;
    if(count >= 524288) {
        return recursive_pivot_sample(values, first, first + step * 4, first + step * 7, step, compare);
    }

    let left = median_index(values, first, first + step, first + step * 2, compare);
    let center = median_index(values, middle - step, middle, middle + step, compare);
    let right = median_index(values, last - 1 - step * 2, last - 1 - step, last - 1, compare);
    return median_index(values, left, center, right, compare);
}

partition_two_way<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, last: usize, pivot_index: usize, compare: Compare) -> unstable_partition
{
    swap_values(values, first, pivot_index);

    let data = values.data();
    let left = first + 1;
    let right = last;
    while(true) {
        while(left < right and compare(*(data + left), *(data + first))) {
            left += 1;
        }
        while(left < right and not compare(*(data + right - 1), *(data + first))) {
            right -= 1;
        }
        if(left >= right) {
            break;
        }
        right -= 1;
        swap_values(values, left, right);
        left += 1;
    }

    let pivot = left - 1;
    swap_values(values, first, pivot);
    return unstable_partition{ .less_end = pivot, .greater_start = pivot + 1 };
}

partition_three_way<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, last: usize, pivot_index: usize, compare: Compare) -> unstable_partition
{
    swap_values(values, first, pivot_index);

    let data = values.data();
    let less = first;
    let index = first + 1;
    let greater = last;
    while(index < greater) {
        if(compare(*(data + index), *(data + first))) {
            less += 1;
            swap_values(values, less, index);
            index += 1;
        } else if(compare(*(data + first), *(data + index))) {
            greater -= 1;
            swap_values(values, index, greater);
        } else {
            index += 1;
        }
    }

    swap_values(values, first, less);
    return unstable_partition{ .less_end = less, .greater_start = greater };
}

heap_sift_down<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, root: usize, count: usize, compare: Compare) -> void
{
    let data = values.data();
    let current = root;
    while(current * 2 + 1 < count) {
        let child = current * 2 + 1;
        let candidate = current;
        if(compare(*(data + first + candidate), *(data + first + child))) {
            candidate = child;
        }
        if(child + 1 < count and compare(*(data + first + candidate), *(data + first + child + 1))) {
            candidate = child + 1;
        }
        if(candidate == current) {
            return;
        }
        swap_values(values, first + current, first + candidate);
        current = candidate;
    }
}

heap_sort_range<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, last: usize, compare: Compare) -> void
{
    let count = last - first;
    if(count < 2) {
        return;
    }

    let heap_start = count / 2;
    while(heap_start > 0) {
        heap_start -= 1;
        heap_sift_down(values, first, heap_start, count, compare);
    }

    let heap_end = count;
    while(heap_end > 1) {
        heap_end -= 1;
        swap_values(values, first, first + heap_end);
        heap_sift_down(values, first, 0 as usize, heap_end, compare);
    }
}

unstable_quick_sort<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, first: usize, last: usize, bad_allowed: usize, compare: Compare) -> void
{
    let left = first;
    let right = last;
    let limit = bad_allowed;
    while(right - left > 12) {
        if(limit == 0) {
            heap_sort_range(values, left, right, compare);
            return;
        }

        let count = right - left;
        let pivot_index = choose_unstable_pivot(values, left, right, compare);
        let partition = unstable_partition{ .less_end = 0 as usize, .greater_start = 0 as usize };
        if(sample_has_duplicates(values, left, right, compare)) {
            partition = partition_three_way(values, left, right, pivot_index, compare);
        } else {
            partition = partition_two_way(values, left, right, pivot_index, compare);
            if(partition.less_end == left or partition.greater_start == right) {
                partition = partition_three_way(values, left, right, partition.less_end, compare);
            }
        }
        let left_len = partition.less_end - left;
        let right_len = right - partition.greater_start;
        if(left_len < count / 8 or right_len < count / 8) {
            limit -= 1;
        }

        if(left_len < right_len) {
            unstable_quick_sort(values, left, partition.less_end, limit, compare);
            left = partition.greater_start;
        } else {
            unstable_quick_sort(values, partition.greater_start, right, limit, compare);
            right = partition.less_end;
        }
    }

    binary_insertion_sort(values, left, right, left, compare);
}

unstable_sort_range<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, compare: Compare) -> void
{
    let count = values.size();
    if(count < 2 or finish_if_monotonic(values, compare)) {
        return;
    }

    unstable_quick_sort(values, 0 as usize, count, floor_log2(count) * 2, compare);
}

sample_has_long_local_runs<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, compare: Compare) -> bool
{
    let count = values.size();
    let block_count = min_usize(16 as usize, count);
    let block_len = min_usize(64 as usize, count);
    let ascending: usize = 0;
    let total: usize = 0;
    let block = 0 as usize;
    while(block < block_count) {
        let start = (block * count) / block_count;
        let stop = min_usize(start + block_len, count);
        let index = start + 1;
        while(index < stop) {
            if(not compare(values[index], values[index - 1])) {
                ascending += 1;
            }
            total += 1;
            index += 1;
        }
        block += 1;
    }

    return total > 0 as usize and ascending * 4 >= total * 3;
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
        construct_at(temp.data() + index, move values[left + index]);
        index += 1;
    }

    let temp_index: usize = 0;
    let right_index = right;
    let write_index = left;
    while(temp_index < left_len and right_index < right + right_len) {
        if(compare(values[right_index], *(temp.data() + temp_index))) {
            values[write_index] = move values[right_index];
            right_index += 1;
        } else {
            values[write_index] = move *(temp.data() + temp_index);
            temp_index += 1;
        }
        write_index += 1;
    }

    while(temp_index < left_len) {
        values[write_index] = move *(temp.data() + temp_index);
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
        construct_at(temp.data() + index, move values[right + index]);
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
            values[write_index] = move values[left_index];
            left_remaining -= 1;
        } else {
            values[write_index] = move *(temp.data() + temp_index);
            temp_remaining -= 1;
        }
    }

    while(temp_remaining > 0) {
        temp_remaining -= 1;
        write_index -= 1;
        values[write_index] = move *(temp.data() + temp_remaining);
    }

    index = 0;
    while(index < right_len) {
        destroy_at(temp.data() + index);
        index += 1;
    }
}

stable_sort_merge<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, temp: raw_buffer<T>&, left: usize, left_len: usize, right: usize, right_len: usize, compare: Compare) -> void
{
    if(left_len <= right_len) {
        merge_low(values, temp, left, left_len, right, right_len, compare);
    } else {
        merge_high(values, temp, left, left_len, right, right_len, compare);
    }
}

power_sort_merge_runs<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, temp: raw_buffer<T>&, left: power_sort_run, right: power_sort_run, compare: Compare) -> power_sort_run
{
    if(compare(values[right.start], values[right.start - 1])) {
        stable_sort_merge(values, temp, left.start, left.len, right.start, right.len, compare);
    }
    return power_sort_run{ .start = left.start, .len = left.len + right.len, .power = 0 };
}

power_sort_range<T: mutable_object, Compare: strict_weak_order<T>>(values: span<T>, compare: Compare) -> void
{
    let count = values.size();
    if(count < 2) {
        return;
    }

    if(count < 64) {
        binary_insertion_sort(values, 0 as usize, count, 1 as usize, compare);
        return;
    }

    let min_run = stable_sort_min_run(count);
    let temp = raw_buffer<T>{count};
    let pending = vector<power_sort_run>{};
    let current: usize = 0;
    let remaining = count;
    let run_len = count_run_and_make_ascending(values, current, count, compare);
    if(run_len < min_run) {
        let forced_len = min_usize(min_run, remaining);
        binary_insertion_sort(values, current, current + forced_len, current + run_len, compare);
        run_len = forced_len;
    }
    let previous = power_sort_run{ .start = current, .len = run_len, .power = 0 };
    current += run_len;
    remaining -= run_len;

    while(remaining > 0) {
        run_len = count_run_and_make_ascending(values, current, count, compare);
        if(run_len < min_run) {
            let forced_len = min_usize(min_run, remaining);
            binary_insertion_sort(values, current, current + forced_len, current + run_len, compare);
            run_len = forced_len;
        }

        let next = power_sort_run{ .start = current, .len = run_len, .power = 0 };
        let power = power_sort_node_power(count, previous, next);
        while(not pending.empty() and pending.back().power > power) {
            let left = pending.pop();
            previous = power_sort_merge_runs(values, temp, left, previous, compare);
        }

        previous.power = power;
        pending.push_back(previous);
        previous = next;
        current += run_len;
        remaining -= run_len;
    }

    while(not pending.empty()) {
        let left = pending.pop();
        previous = power_sort_merge_runs(values, temp, left, previous, compare);
    }
}

sort_span<T: mutable_object, Compare: strict_weak_order<T> = less<T>>(values: span<T>, compare: Compare = Compare{}) -> void
{
    let count = values.size();
    if(count < 2 or finish_if_monotonic(values, compare)) {
        return;
    }

    if(count <= 1024 or sample_has_long_local_runs(values, compare)) {
        power_sort_range(values, compare);
        return;
    }

    unstable_quick_sort(values, 0 as usize, count, floor_log2(count) * 2, compare);
}

stable_sort_span<T: mutable_object, Compare: strict_weak_order<T> = less<T>>(values: span<T>, compare: Compare = Compare{}) -> void
{
    power_sort_range(values, compare);
}

export sort<R: contiguous_mutable_range, Compare: strict_weak_order<R::item> = less<R::item>>(values: R forward&, compare: Compare = Compare{}) -> void
{
    sort_span(span<R::item>{values.data(), values.size()}, compare);
}

export stable_sort<R: contiguous_mutable_range, Compare: strict_weak_order<R::item> = less<R::item>>(values: R forward&, compare: Compare = Compare{}) -> void
{
    stable_sort_span(span<R::item>{values.data(), values.size()}, compare);
}
