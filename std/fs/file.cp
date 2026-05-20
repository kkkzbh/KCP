export module std.fs.file;

import std.detail.runtime;
import std.core.expected;
import std.memory.span;
import std.text.str;

export enum io_error : u8 {
    open_failed = 1;
    read_failed = 2;
    write_failed = 3;
    close_failed = 4;
}

export enum open_flag : u8 {
    read = 1 << 0;
    write = 1 << 1;
    create = 1 << 2;
    truncate = 1 << 3;
    append = 1 << 4;
}

export type file_handle = opaque u8*;
export type open_options = opaque u8;

impl open_options {
    bits(self) -> u8
    {
        return self as u8;
    }

    with(self, flag: open_flag) -> open_options
    {
        return ((self as u8) | (flag as u8)) as open_options;
    }

    read(self) -> open_options
    {
        return self.with(open_flag::read);
    }

    write(self) -> open_options
    {
        return self.with(open_flag::write);
    }

    create(self) -> open_options
    {
        return self.with(open_flag::create);
    }

    truncate(self) -> open_options
    {
        return self.with(open_flag::truncate);
    }

    append(self) -> open_options
    {
        return self.with(open_flag::append);
    }
}

export struct file {
    handle: file_handle;
}

impl file {
    file(handle: file_handle)
    {
        return file{ .handle = handle };
    }

    file(other: this const&) = delete;
    operator =(self&, rhs: this const&) = delete;

    file(other: this move&)
    {
        let result = file{ .handle = other.handle };
        other.handle = (nullptr as u8*) as file_handle;
        return result;
    }

    operator =(self&, rhs: this move&) -> this&
    {
        if((handle as u8*) != (rhs.handle as u8*)) {
            close();
            handle = rhs.handle;
            rhs.handle = (nullptr as u8*) as file_handle;
        }
        return ref self;
    }

    ~file()
    {
        close();
    }

    open(path: str, options: open_options) -> expected<file, io_error>
    {
        let raw = cp_file_open(path.data(), path.size(), options.bits());
        if(raw == nullptr) {
            return expected<file, io_error>::unexpected(io_error::open_failed);
        }
        return expected<file, io_error>::value(file{ .handle = raw as file_handle });
    }

    close(self&) -> expected<usize, io_error>
    {
        let raw = handle as u8*;
        if(raw == nullptr) {
            return expected<usize, io_error>::value(0);
        }
        handle = (nullptr as u8*) as file_handle;
        if(cp_file_close(raw) != 0) {
            return expected<usize, io_error>::unexpected(io_error::close_failed);
        }
        return expected<usize, io_error>::value(0);
    }

    read(self&, out: span<u8>) -> expected<usize, io_error>
    {
        let out_len: usize = 0;
        if(cp_file_read(handle as u8*, out.data(), out.size(), &out_len) != 0) {
            return expected<usize, io_error>::unexpected(io_error::read_failed);
        }
        return expected<usize, io_error>::value(out_len);
    }

    write(self&, data: span<u8> const&) -> expected<usize, io_error>
    {
        let out_len: usize = 0;
        if(cp_file_write(handle as u8*, data.data(), data.size(), &out_len) != 0) {
            return expected<usize, io_error>::unexpected(io_error::write_failed);
        }
        return expected<usize, io_error>::value(out_len);
    }

    write_str(self&, text: str) -> expected<usize, io_error>
    {
        let out_len: usize = 0;
        if(cp_file_write(handle as u8*, text.data() as u8 const*, text.size(), &out_len) != 0) {
            return expected<usize, io_error>::unexpected(io_error::write_failed);
        }
        return expected<usize, io_error>::value(out_len);
    }
}
