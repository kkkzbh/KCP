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

ordered_before<T: mutable_object, Order: ordering<T>>(left: T const&, right: T const&, order: Order) -> bool
{
    return is_less(order(left, right));
}

finish_if_monotonic<T: mutable_object, Order: ordering<T>>(values: span<T>, order: Order) -> bool
{
    let count = values.size();
    if(count < 2) {
        return true;
    }

    let data = values.data();
    let index: usize = 1;
    while(index < count and not ordered_before(*(data + index), *(data + index - 1), order)) {
        index += 1;
    }
    if(index == count) {
        return true;
    }

    index = 1;
    while(index < count and ordered_before(*(data + index), *(data + index - 1), order)) {
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

binary_insertion_sort<T: mutable_object, Order: ordering<T>>(values: span<T>, first: usize, last: usize, sorted_until: usize, order: Order) -> void
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
            if(ordered_before(item, *(data + mid), order)) {
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

median_index<T: mutable_object, Order: ordering<T>>(values: span<T>, first: usize, middle: usize, last: usize, order: Order) -> usize
{
    let data = values.data();
    if(ordered_before(*(data + middle), *(data + first), order)) {
        if(ordered_before(*(data + last), *(data + middle), order)) {
            return middle;
        }
        if(ordered_before(*(data + last), *(data + first), order)) {
            return last;
        }
        return first;
    }
    if(ordered_before(*(data + last), *(data + first), order)) {
        return first;
    }
    if(ordered_before(*(data + last), *(data + middle), order)) {
        return last;
    }
    return middle;
}

equivalent_values<T: mutable_object, Order: ordering<T>>(values: span<T>, left: usize, right: usize, order: Order) -> bool
{
    let data = values.data();
    return is_equivalent(order(*(data + left), *(data + right)));
}

sample_has_duplicates<T: mutable_object, Order: ordering<T>>(values: span<T>, first: usize, last: usize, order: Order) -> bool
{
    let count = last - first;
    let middle = first + count / 2;
    if(equivalent_values(values, first, middle, order) or equivalent_values(values, middle, last - 1, order) or equivalent_values(values, first, last - 1, order)) {
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
    return equivalent_values(values, first, a, order)
        or equivalent_values(values, first, b, order)
        or equivalent_values(values, a, b, order)
        or equivalent_values(values, c, middle, order)
        or equivalent_values(values, c, d, order)
        or equivalent_values(values, middle, d, order)
        or equivalent_values(values, e, f, order)
        or equivalent_values(values, e, last - 1, order)
        or equivalent_values(values, f, last - 1, order);
}

recursive_pivot_sample<T: mutable_object, Order: ordering<T>>(values: span<T>, first: usize, middle: usize, last: usize, count: usize, order: Order) -> usize
{
    let left = first;
    let center = middle;
    let right = last;
    if(count * 8 >= 64) {
        let step = count / 8;
        left = recursive_pivot_sample(values, left, left + step * 4, left + step * 7, step, order);
        center = recursive_pivot_sample(values, center, center + step * 4, center + step * 7, step, order);
        right = recursive_pivot_sample(values, right, right + step * 4, right + step * 7, step, order);
    }

    return median_index(values, left, center, right, order);
}

choose_unstable_pivot<T: mutable_object, Order: ordering<T>>(values: span<T>, first: usize, last: usize, order: Order) -> usize
{
    let count = last - first;
    let middle = first + count / 2;
    if(count < 128) {
        return median_index(values, first, middle, last - 1, order);
    }

    let step = count / 8;
    if(count >= 524288) {
        return recursive_pivot_sample(values, first, first + step * 4, first + step * 7, step, order);
    }

    let left = median_index(values, first, first + step, first + step * 2, order);
    let center = median_index(values, middle - step, middle, middle + step, order);
    let right = median_index(values, last - 1 - step * 2, last - 1 - step, last - 1, order);
    return median_index(values, left, center, right, order);
}

partition_two_way<T: mutable_object, Order: ordering<T>>(values: span<T>, first: usize, last: usize, pivot_index: usize, order: Order) -> unstable_partition
{
    swap_values(values, first, pivot_index);

    let data = values.data();
    let left = first + 1;
    let right = last;
    while(true) {
        while(left < right and ordered_before(*(data + left), *(data + first), order)) {
            left += 1;
        }
        while(left < right and not ordered_before(*(data + right - 1), *(data + first), order)) {
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

partition_three_way<T: mutable_object, Order: ordering<T>>(values: span<T>, first: usize, last: usize, pivot_index: usize, order: Order) -> unstable_partition
{
    swap_values(values, first, pivot_index);

    let data = values.data();
    let less = first;
    let index = first + 1;
    let greater = last;
    while(index < greater) {
        let relation = order(*(data + index), *(data + first));
        if(is_less(relation)) {
            less += 1;
            swap_values(values, less, index);
            index += 1;
        } else if(is_greater(relation)) {
            greater -= 1;
            swap_values(values, index, greater);
        } else {
            index += 1;
        }
    }

    swap_values(values, first, less);
    return unstable_partition{ .less_end = less, .greater_start = greater };
}

heap_sift_down<T: mutable_object, Order: ordering<T>>(values: span<T>, first: usize, root: usize, count: usize, order: Order) -> void
{
    let data = values.data();
    let current = root;
    while(current * 2 + 1 < count) {
        let child = current * 2 + 1;
        let candidate = current;
        if(ordered_before(*(data + first + candidate), *(data + first + child), order)) {
            candidate = child;
        }
        if(child + 1 < count and ordered_before(*(data + first + candidate), *(data + first + child + 1), order)) {
            candidate = child + 1;
        }
        if(candidate == current) {
            return;
        }
        swap_values(values, first + current, first + candidate);
        current = candidate;
    }
}

heap_sort_range<T: mutable_object, Order: ordering<T>>(values: span<T>, first: usize, last: usize, order: Order) -> void
{
    let count = last - first;
    if(count < 2) {
        return;
    }

    let heap_start = count / 2;
    while(heap_start > 0) {
        heap_start -= 1;
        heap_sift_down(values, first, heap_start, count, order);
    }

    let heap_end = count;
    while(heap_end > 1) {
        heap_end -= 1;
        swap_values(values, first, first + heap_end);
        heap_sift_down(values, first, 0 as usize, heap_end, order);
    }
}

unstable_quick_sort<T: mutable_object, Order: ordering<T>>(values: span<T>, first: usize, last: usize, bad_allowed: usize, order: Order) -> void
{
    let left = first;
    let right = last;
    let limit = bad_allowed;
    while(right - left > 12) {
        if(limit == 0) {
            heap_sort_range(values, left, right, order);
            return;
        }

        let count = right - left;
        let pivot_index = choose_unstable_pivot(values, left, right, order);
        let partition = unstable_partition{ .less_end = 0 as usize, .greater_start = 0 as usize };
        if(sample_has_duplicates(values, left, right, order)) {
            partition = partition_three_way(values, left, right, pivot_index, order);
        } else {
            partition = partition_two_way(values, left, right, pivot_index, order);
            if(partition.less_end == left or partition.greater_start == right) {
                partition = partition_three_way(values, left, right, partition.less_end, order);
            }
        }
        let left_len = partition.less_end - left;
        let right_len = right - partition.greater_start;
        if(left_len < count / 8 or right_len < count / 8) {
            limit -= 1;
        }

        if(left_len < right_len) {
            unstable_quick_sort(values, left, partition.less_end, limit, order);
            left = partition.greater_start;
        } else {
            unstable_quick_sort(values, partition.greater_start, right, limit, order);
            right = partition.less_end;
        }
    }

    binary_insertion_sort(values, left, right, left, order);
}

unstable_sort_range<T: mutable_object, Order: ordering<T>>(values: span<T>, order: Order) -> void
{
    let count = values.size();
    if(count < 2 or finish_if_monotonic(values, order)) {
        return;
    }

    unstable_quick_sort(values, 0 as usize, count, floor_log2(count) * 2, order);
}

sample_has_long_local_runs<T: mutable_object, Order: ordering<T>>(values: span<T>, order: Order) -> bool
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
            if(not ordered_before(values[index], values[index - 1], order)) {
                ascending += 1;
            }
            total += 1;
            index += 1;
        }
        block += 1;
    }

    return total > 0 as usize and ascending * 4 >= total * 3;
}

count_run_and_make_ascending<T: mutable_object, Order: ordering<T>>(values: span<T>, first: usize, last: usize, order: Order) -> usize
{
    let current = first + 1;
    if(current == last) {
        return 1;
    }

    current += 1;
    if(ordered_before(values[first + 1], values[first], order)) {
        while(current < last and ordered_before(values[current], values[current - 1], order)) {
            current += 1;
        }
        reverse_range(values, first, current);
    } else {
        while(current < last and not ordered_before(values[current], values[current - 1], order)) {
            current += 1;
        }
    }

    return current - first;
}

merge_low<T: mutable_object, Order: ordering<T>>(values: span<T>, temp: raw_buffer<T>&, left: usize, left_len: usize, right: usize, right_len: usize, order: Order) -> void
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
        if(ordered_before(values[right_index], *(temp.data() + temp_index), order)) {
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

merge_high<T: mutable_object, Order: ordering<T>>(values: span<T>, temp: raw_buffer<T>&, left: usize, left_len: usize, right: usize, right_len: usize, order: Order) -> void
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
        if(ordered_before(*(temp.data() + temp_index), values[left_index], order)) {
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

stable_sort_merge<T: mutable_object, Order: ordering<T>>(values: span<T>, temp: raw_buffer<T>&, left: usize, left_len: usize, right: usize, right_len: usize, order: Order) -> void
{
    if(left_len <= right_len) {
        merge_low(values, temp, left, left_len, right, right_len, order);
    } else {
        merge_high(values, temp, left, left_len, right, right_len, order);
    }
}

power_sort_merge_runs<T: mutable_object, Order: ordering<T>>(values: span<T>, temp: raw_buffer<T>&, left: power_sort_run, right: power_sort_run, order: Order) -> power_sort_run
{
    if(ordered_before(values[right.start], values[right.start - 1], order)) {
        stable_sort_merge(values, temp, left.start, left.len, right.start, right.len, order);
    }
    return power_sort_run{ .start = left.start, .len = left.len + right.len, .power = 0 };
}

power_sort_range<T: mutable_object, Order: ordering<T>>(values: span<T>, order: Order) -> void
{
    let count = values.size();
    if(count < 2) {
        return;
    }

    if(count < 64) {
        binary_insertion_sort(values, 0 as usize, count, 1 as usize, order);
        return;
    }

    let min_run = stable_sort_min_run(count);
    let temp = raw_buffer<T>{count};
    let pending = vector<power_sort_run>{};
    let current: usize = 0;
    let remaining = count;
    let run_len = count_run_and_make_ascending(values, current, count, order);
    if(run_len < min_run) {
        let forced_len = min_usize(min_run, remaining);
        binary_insertion_sort(values, current, current + forced_len, current + run_len, order);
        run_len = forced_len;
    }
    let previous = power_sort_run{ .start = current, .len = run_len, .power = 0 };
    current += run_len;
    remaining -= run_len;

    while(remaining > 0) {
        run_len = count_run_and_make_ascending(values, current, count, order);
        if(run_len < min_run) {
            let forced_len = min_usize(min_run, remaining);
            binary_insertion_sort(values, current, current + forced_len, current + run_len, order);
            run_len = forced_len;
        }

        let next = power_sort_run{ .start = current, .len = run_len, .power = 0 };
        let power = power_sort_node_power(count, previous, next);
        while(not pending.empty() and pending.back().power > power) {
            let left = pending.pop();
            previous = power_sort_merge_runs(values, temp, left, previous, order);
        }

        previous.power = power;
        pending.push_back(previous);
        previous = next;
        current += run_len;
        remaining -= run_len;
    }

    while(not pending.empty()) {
        let left = pending.pop();
        previous = power_sort_merge_runs(values, temp, left, previous, order);
    }
}

sort_span<T: mutable_object, Order: ordering<T> = asc<T>>(values: span<T>, order: Order = Order{}) -> void
{
    let count = values.size();
    if(count < 2 or finish_if_monotonic(values, order)) {
        return;
    }

    if(count <= 1024 or sample_has_long_local_runs(values, order)) {
        power_sort_range(values, order);
        return;
    }

    unstable_quick_sort(values, 0 as usize, count, floor_log2(count) * 2, order);
}

stable_sort_span<T: mutable_object, Order: ordering<T> = asc<T>>(values: span<T>, order: Order = Order{}) -> void
{
    power_sort_range(values, order);
}

export sort<R: contiguous_mutable_range, Order: ordering<R::item> = asc<R::item>>(values: R forward&, order: Order = Order{}) -> void
{
    sort_span(span<R::item>{values.data(), values.size()}, order);
}

export stable_sort<R: contiguous_mutable_range, Order: ordering<R::item> = asc<R::item>>(values: R forward&, order: Order = Order{}) -> void
{
    stable_sort_span(span<R::item>{values.data(), values.size()}, order);
}
