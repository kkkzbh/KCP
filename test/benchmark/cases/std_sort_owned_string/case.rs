use std::env;

fn repeat_count(count: i32) -> i32 {
    if count <= 1024 {
        return 256;
    }
    if count <= 8192 {
        return 24;
    }
    4
}

fn stable_requested() -> bool {
    env::var("CP_BENCH_STABLE").is_ok()
}

fn word_at(value: i32) -> String {
    const WORDS: [&str; 32] = [
        "xenon_delta", "amber_flux", "violet_cedar", "cobalt_ember",
        "silver_mint", "orange_slate", "brass_lumen", "indigo_river",
        "pearl_vector", "hazel_matrix", "crimson_node", "opal_kernel",
        "saffron_loop", "teal_packet", "garnet_cache", "ivory_branch",
        "jade_signal", "maroon_stack", "navy_cursor", "olive_thread",
        "plum_object", "quartz_range", "ruby_token", "topaz_scope",
        "umber_arena", "vermilion_ir", "walnut_parse", "yellow_type",
        "zircon_alloc", "azure_merge", "bronze_sort", "coral_span",
    ];
    WORDS[(value % WORDS.len() as i32) as usize].to_owned()
}

fn letter_score(ch: u8) -> i64 {
    if ch == b'_' {
        return 27;
    }
    (ch - b'a' + 1) as i64
}

fn word_score(value: &str) -> i64 {
    let mut total = 0i64;
    for ch in value.bytes() {
        total = (total * 31 + letter_score(ch)) % 1_000_000_007;
    }
    total
}

fn checked_checksum(values: &[String]) -> i64 {
    let mut total = 0i64;
    let mut index = 0usize;
    while index < values.len() {
        if index > 0 && values[index] < values[index - 1] {
            return -1;
        }
        total = (total + word_score(&values[index]) * (index % 251 + 1) as i64) % 1_000_000_007;
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

    let mut original = Vec::<String>::with_capacity(count as usize);
    let mut index = 0i32;
    while index < count {
        original.push(word_at((index * 4827 + count * 97) % 1_000_003));
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
