use std::env;

fn main() {
    let limit = env::var("CP_BENCH_INPUT")
        .ok()
        .and_then(|value| value.parse::<i64>().ok())
        .unwrap_or(1);
    let mut index = 0i64;
    let mut total = 7i64;

    while index < limit {
        total = (total + ((index * 31 + 17) % 1009)) % 1_000_003;
        index += 1;
    }

    std::process::exit((total % 251) as i32);
}
