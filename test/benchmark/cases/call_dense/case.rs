use std::env;

fn step_a(value: i32) -> i32 {
    (value * 3 + 1) % 1009
}

fn step_b(value: i32) -> i32 {
    (step_a(value) + step_a(value + 7)) % 1009
}

fn step_c(value: i32) -> i32 {
    (step_b(value) * 5 + step_a(value - 3)) % 1009
}

fn main() {
    let count = env::var("CP_BENCH_INPUT")
        .ok()
        .and_then(|value| value.parse::<i32>().ok())
        .unwrap_or(1);
    let mut index = 0i32;
    let mut total = 0i32;
    while index < count {
        total = (total + step_c(index)) % 1_000_003;
        index += 1;
    }
    std::process::exit(total % 251);
}
