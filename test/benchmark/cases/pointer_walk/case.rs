use std::env;

fn main() {
    let count = env::var("CP_BENCH_INPUT")
        .ok()
        .and_then(|value| value.parse::<usize>().ok())
        .unwrap_or(1);
    let mut values = vec![0i32; count];
    let mut index = 0usize;
    while index < count {
        values[index] = ((index as i32) * 17 + 3) % 1009;
        index += 1;
    }

    let mut total = 0i64;
    let mut cursor = 0usize;
    while cursor < values.len() {
        total += values[cursor] as i64;
        cursor += 1;
    }

    std::process::exit((total % 251) as i32);
}
