import std;

main() -> i32
{
    let path: str = "/tmp/cp-example-fs.txt";
    match file::open(path, open_options{}.write().create().truncate()) {
        .value(out) => {
            let written = out.write_str("cp");
            if(written.value_or(0 as usize) != (2 as usize)) {
                return 1;
            }
            out.close();
        },
        .unexpected(error) => {
            return 2;
        },
    };

    match file::open(path, open_options{}.read()) {
        .value(input) => {
            let storage = buffer<u8>{2};
            let read = input.read(span<u8>{storage.data(), 2});
            if(read.value_or(0 as usize) != (2 as usize)) {
                return 3;
            }
            return (*(storage.data()) as i32) - 99;
        },
        .unexpected(error) => {
            return 4;
        },
    };
}
