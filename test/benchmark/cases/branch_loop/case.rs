use std::env;

fn main() {
    let limit = env::var("CP_BENCH_INPUT")
        .ok()
        .and_then(|value| value.parse::<i32>().ok())
        .unwrap_or(1);
    let mut index = 0i32;
    let mut total = 0i32;

    while index < limit {
        if index % 7 == 0 {
            total += 3;
        } else if index % 5 == 0 {
            total += 2;
        } else {
            total += 1;
        }
        index += 1;
    }

    std::process::exit(total % 251);
}
