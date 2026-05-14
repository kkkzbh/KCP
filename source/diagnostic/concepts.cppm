export module source.diagnostic.concepts;

import std;

export template<typename T>
concept diagnostic_enum = std::is_enum_v<T>;
