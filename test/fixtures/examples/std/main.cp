import std;

main() -> i32
{
    let some = optional<i32>::some(20);
    let none = optional<i32>::none;
    let value = expected<i32,str>::value(12);
    let error = expected<i32,str>::unexpected("bad");
    let indices = iota(0, 4);
    let storage = raw_buffer<i32>{2};
    let values = vector<i32>{};
    let ids = map<i32, i32>{};
    let keys = set<i32>{};
    let text = string{"hi"};
    let total = 0;

    construct_at(storage.data(), 5);
    construct_at(storage.data() + 1, 7);

    values.push_back(3);
    values.push_back(4);
    values.insert(1 as usize, 5);
    values.erase(0 as usize);
    let vector_sum = values[0 as usize] + values.pop();
    ids.insert(2, 20);
    ids[1] = 10;
    ids.insert(2, 99);
    keys.insert(4);
    keys.insert(2);
    keys.insert(4);
    let map_set_sum = ids.at(1) + ids.at(2) + ids.nth(0 as usize).value + keys.nth(0 as usize) + keys.rank(4) as i32;
    text.push_back('!');
    text.append(" cp");
    text[0 as usize] = 'H';
    let string_size = text.size() as i32;
    let range_sum = iota(0, 8)
        .filter(f(index: i32) -> bool { return index != 3; })
        .transform(f(index: i32) -> i32 { return index + 1; })
        .take(4 as usize)
        .fold(0, f(total: i32, value: i32) -> i32 { return total + value; });
    let repeated_count = repeat(1).take(5 as usize).count() as i32;
    let borrowed_values: [i32; 3] = [1, 2, 3];
    let borrowed_count = all(ref borrowed_values)
        .filter(f(value: i32&) -> bool { return value > 1; })
        .count() as i32;
    let mapper = f(value: i32) -> i32 { return value + 1; };
    type mapped = call_result<decltype(mapper), i32>;
    let meta_value: mapped = mapper(0);

    for(let index : indices) {
        total += index;
    }

    if(some.has_value() and not none.has_value() and value.has_value() and not error.has_value()) {
        let stored = *storage.data() + *(storage.data() + 1);
        println("std = {}, stored = {}", "ok", stored);
        println("owned string = {}", text);
        destroy_at(storage.data() + 1);
        destroy_at(storage.data());
        return some.value_or(0) + none.value_or(10) + value.value_or(0) + total + stored + vector_sum + string_size + map_set_sum
            + range_sum + repeated_count + borrowed_count + meta_value;
    }
    return 1;
}
