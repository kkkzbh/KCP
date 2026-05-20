export module std.detail.runtime;

export extern "C" cp_io_write_char(stream: i32, ch: char) -> i32;
export extern "C" cp_file_open(path: char const*, path_len: usize, flags: u8) -> u8*;
export extern "C" cp_file_close(handle: u8*) -> i32;
export extern "C" cp_file_read(handle: u8*, data: u8*, len: usize, out_len: usize*) -> i32;
export extern "C" cp_file_write(handle: u8*, data: u8 const*, len: usize, out_len: usize*) -> i32;
