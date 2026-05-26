import std.algorithm.sort;
import std.collections.vector;
import std.text.str;
import std.text.string;

extern "C" getenv(name: char const*) -> char*;
extern "C" atoi(text: char const*) -> i32;

bench_input() -> i32
{
    let name = "CP_BENCH_INPUT";
    let raw = getenv(name.data());
    if(raw == nullptr) {
        return 1;
    }
    return atoi(raw);
}

stable_requested() -> bool
{
    let name = "CP_BENCH_STABLE";
    return getenv(name.data()) != nullptr;
}

repeat_count(count: i32) -> i32
{
    if(count <= 1024) {
        return 256;
    }
    if(count <= 8192) {
        return 24;
    }
    return 4;
}

word_at(value: i32) -> str
{
    let code = value % 32;
    if(code == 0) { return "xenon_delta"; }
    if(code == 1) { return "amber_flux"; }
    if(code == 2) { return "violet_cedar"; }
    if(code == 3) { return "cobalt_ember"; }
    if(code == 4) { return "silver_mint"; }
    if(code == 5) { return "orange_slate"; }
    if(code == 6) { return "brass_lumen"; }
    if(code == 7) { return "indigo_river"; }
    if(code == 8) { return "pearl_vector"; }
    if(code == 9) { return "hazel_matrix"; }
    if(code == 10) { return "crimson_node"; }
    if(code == 11) { return "opal_kernel"; }
    if(code == 12) { return "saffron_loop"; }
    if(code == 13) { return "teal_packet"; }
    if(code == 14) { return "garnet_cache"; }
    if(code == 15) { return "ivory_branch"; }
    if(code == 16) { return "jade_signal"; }
    if(code == 17) { return "maroon_stack"; }
    if(code == 18) { return "navy_cursor"; }
    if(code == 19) { return "olive_thread"; }
    if(code == 20) { return "plum_object"; }
    if(code == 21) { return "quartz_range"; }
    if(code == 22) { return "ruby_token"; }
    if(code == 23) { return "topaz_scope"; }
    if(code == 24) { return "umber_arena"; }
    if(code == 25) { return "vermilion_ir"; }
    if(code == 26) { return "walnut_parse"; }
    if(code == 27) { return "yellow_type"; }
    if(code == 28) { return "zircon_alloc"; }
    if(code == 29) { return "azure_merge"; }
    if(code == 30) { return "bronze_sort"; }
    return "coral_span";
}

letter_score(ch: char) -> i64
{
    if(ch == '_') { return 27; }
    if(ch == 'a') { return 1; }
    if(ch == 'b') { return 2; }
    if(ch == 'c') { return 3; }
    if(ch == 'd') { return 4; }
    if(ch == 'e') { return 5; }
    if(ch == 'f') { return 6; }
    if(ch == 'g') { return 7; }
    if(ch == 'h') { return 8; }
    if(ch == 'i') { return 9; }
    if(ch == 'j') { return 10; }
    if(ch == 'k') { return 11; }
    if(ch == 'l') { return 12; }
    if(ch == 'm') { return 13; }
    if(ch == 'n') { return 14; }
    if(ch == 'o') { return 15; }
    if(ch == 'p') { return 16; }
    if(ch == 'q') { return 17; }
    if(ch == 'r') { return 18; }
    if(ch == 's') { return 19; }
    if(ch == 't') { return 20; }
    if(ch == 'u') { return 21; }
    if(ch == 'v') { return 22; }
    if(ch == 'w') { return 23; }
    if(ch == 'x') { return 24; }
    if(ch == 'y') { return 25; }
    return 26;
}

word_score(value: str) -> i64
{
    let total: i64 = 0;
    let index: usize = 0;
    while(index < value.size()) {
        total = (total * 31 + letter_score(value[index])) % 1000000007;
        index += 1;
    }
    return total;
}

checked_checksum(values: vector<string> const&) -> i64
{
    let total: i64 = 0;
    let index: usize = 0;
    while(index < values.size()) {
        if(index > 0 as usize and values[index].as_str() < values[index - 1].as_str()) {
            return -1;
        }
        total = (total + word_score(values[index].as_str()) * ((index % 251 + 1) as i64)) % 1000000007;
        index += 1;
    }
    return total;
}

main() -> i32
{
    let count = bench_input();
    if(count <= 0) {
        return 1;
    }

    let original = vector<string>{};
    original.reserve(count as usize);
    let index = 0;
    while(index < count) {
        original.push_back(string{word_at((index * 4827 + count * 97) % 1000003)});
        index += 1;
    }

    let total: i64 = 0;
    let round = 0;
    let repeats = repeat_count(count);
    let stable = stable_requested();
    while(round < repeats) {
        let values = original;
        if(stable) {
            stable_sort(values, f(left: string const&, right: string const&) -> bool {
                left.as_str() < right.as_str()
            });
        } else {
            sort(values, f(left: string const&, right: string const&) -> bool {
                left.as_str() < right.as_str()
            });
        }
        let checksum = checked_checksum(values);
        if(checksum < 0) {
            return 2;
        }
        total = (total + checksum) % 1000000007;
        round += 1;
    }

    return (total % 251) as i32;
}
