export module semantic.check:concepts;

import std;
import parser;

export template<typename T>
concept semantic_parse_result = std::same_as<std::remove_cvref_t<T>, parse_result>;
