import sample.math;

main() -> i32
{
    let raw = add(16, 26);
    return clamp_min(raw, 0);
}
