use std::collections::{BTreeMap, BTreeSet};
use std::env;

fn main() {
    let count = env::var("CP_BENCH_INPUT")
        .ok()
        .and_then(|value| value.parse::<i32>().ok())
        .unwrap_or(1);
    let mut values = BTreeMap::<i32, i32>::new();
    let mut keys = BTreeSet::<i32>::new();

    let mut index = 0i32;
    while index < count {
        let key = (index * 37) % (count * 2 + 1);
        values.entry(key).or_insert(index % 1009);
        keys.insert(key);
        index += 1;
    }

    let mut total = (values.len() + keys.len()) as i64;
    let mut probe = 0i32;
    while probe < count {
        let key = (probe * 53) % (count * 2 + 1);
        if let Some(value) = values.get(&key) {
            total += *value as i64;
        }
        if keys.contains(&key) {
            total += 3;
        }
        probe += 1;
    }

    std::process::exit((total % 251) as i32);
}
