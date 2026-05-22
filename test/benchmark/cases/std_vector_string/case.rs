use std::env;

fn main() {
    let count = env::var("CP_BENCH_INPUT")
        .ok()
        .and_then(|value| value.parse::<usize>().ok())
        .unwrap_or(1);
    let mut values = Vec::<i32>::with_capacity(count);
    let mut text = String::with_capacity(count);

    let mut index = 0usize;
    while index < count {
        values.push(((index as i32) * 13 + 5) % 997);
        text.push((b'a' + (index % 26) as u8) as char);
        index += 1;
    }

    let mut total = text.len() as i64;
    let mut cursor = 0usize;
    while cursor < values.len() {
        total += values[cursor] as i64;
        cursor += 1;
    }
    std::process::exit((total % 251) as i32);
}
