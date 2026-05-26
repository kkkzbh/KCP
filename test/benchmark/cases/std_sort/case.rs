use std::env;

fn repeat_count(count: i32) -> i32 {
    if count <= 2048 {
        return 512;
    }
    if count <= 65536 {
        return 32;
    }
    2
}

fn stable_requested() -> bool {
    env::var("CP_BENCH_STABLE").is_ok()
}

fn value_at(index: i32, count: i32) -> i32 {
    (((index as i64) * 48_271 + (count as i64) * 17) % 1_000_003) as i32
}

fn checked_checksum(values: &[i32]) -> i64 {
    let mut total = 0i64;
    let mut index = 0usize;
    while index < values.len() {
        if index > 0 && values[index] < values[index - 1] {
            return -1;
        }
        total = (total + values[index] as i64 * (index % 251 + 1) as i64) % 1_000_000_007;
        index += 1;
    }
    total
}

fn main() {
    let count = env::var("CP_BENCH_INPUT")
        .ok()
        .and_then(|value| value.parse::<i32>().ok())
        .unwrap_or(1);
    if count <= 0 {
        std::process::exit(1);
    }

    let mut original = Vec::<i32>::with_capacity(count as usize);
    let mut index = 0i32;
    while index < count {
        original.push(value_at(index, count));
        index += 1;
    }

    let mut total = 0i64;
    let mut round = 0i32;
    let repeats = repeat_count(count);
    let stable = stable_requested();
    while round < repeats {
        let mut values = original.clone();
        if stable {
            values.sort();
        } else {
            values.sort_unstable();
        }
        let checksum = checked_checksum(&values);
        if checksum < 0 {
            std::process::exit(2);
        }
        total = (total + checksum) % 1_000_000_007;
        round += 1;
    }

    std::process::exit((total % 251) as i32);
}
