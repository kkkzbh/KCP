import std;
import source;
import smoke;

auto main() -> int
{
    contract_assert(answer() == 42);

    auto const values = std::vector{1, 2, 3};
    contract_assert(values.size() == 3);

    auto sources = source_manager{};
    auto const first = sources.add_source("first.cp", "let x = 1\nreturn x\n");
    auto const second = sources.add_source("second.cp", "");

    auto const first_start = sources.file_start(first);
    auto const second_start = sources.file_start(second);

    contract_assert(sources.name(first) == "first.cp");
    contract_assert(sources.text(second).empty());
    contract_assert(sources.locate(first_start + 4) == std::pair{ first, byte_pos{ 4 } });
    contract_assert(sources.locate(second_start) == std::pair{ second, byte_pos{ 0 } });
    contract_assert(sources.slice(source_span{ .start = first_start + 4, .end = first_start + 5 }) == "x");
    contract_assert(sources.position(first_start) == source_position{ .line = 1, .column = 1 });
    contract_assert(sources.position(first_start + 10) == source_position{ .line = 2, .column = 1 });
    contract_assert(sources.position(second_start) == source_position{ .line = 1, .column = 1 });

    return 0;
}
