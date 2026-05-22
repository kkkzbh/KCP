use std::env;

fn main() {
    let count = env::var("CP_BENCH_INPUT")
        .ok()
        .and_then(|value| value.parse::<i32>().ok())
        .unwrap_or(1);
    let mut outer = 0i32;
    let mut total = 0i32;
    while outer < count {
        let mut values = vec![0i32; 32];
        let mut index = 0usize;
        while index < 32 {
            values[index] = (outer + (index as i32) * 13) % 997;
            index += 1;
        }
        total = (total + values[(outer % 32) as usize]) % 1_000_003;
        outer += 1;
    }
    std::process::exit(total % 251);
}
