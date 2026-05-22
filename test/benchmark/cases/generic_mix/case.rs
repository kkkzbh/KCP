use std::env;
use std::ops::{Add, Mul, Rem};

fn mix<T>(value: T, salt: T) -> T
where
    T: Copy + From<i32> + Add<Output = T> + Mul<Output = T> + Rem<Output = T>,
{
    (value * T::from(17) + salt) % T::from(1009)
}

fn main() {
    let count = env::var("CP_BENCH_INPUT")
        .ok()
        .and_then(|value| value.parse::<i32>().ok())
        .unwrap_or(1);
    let mut index = 0i32;
    let mut total_i32 = 0i32;
    let mut total_i64 = 0i64;
    while index < count {
        total_i32 = (total_i32 + mix::<i32>(index, 11)) % 1_000_003;
        total_i64 = (total_i64 + mix::<i64>(index as i64, 23)) % 1_000_003;
        index += 1;
    }
    std::process::exit(((total_i32 as i64 + total_i64) % 251) as i32);
}
