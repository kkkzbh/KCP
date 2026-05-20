extern "C" abs(value: i32) -> i32;

enum access_flag : u8 {
    read = 1 << 0;
    write = 1 << 1;
}

type access_mask = opaque u8;

impl access_mask {
    bits(self) -> u8
    {
        return self as u8;
    }

    with(self, flag: access_flag) -> access_mask
    {
        return ((self as u8) | (flag as u8)) as access_mask;
    }
}

main() -> i32
{
    let flags = access_mask{}.with(access_flag::read).with(access_flag::write);
    return abs(-39) + flags.bits() as i32;
}
