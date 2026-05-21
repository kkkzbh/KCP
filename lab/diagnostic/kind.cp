export module diagnostic.kind;

export enum diagnostic_kind : u8 {
    invalid_character = 0;
    invalid_number_suffix = 1;
    leading_zero_integer = 2;
    unterminated_block_comment = 3;
    expected_token = 4;
    expected_identifier = 5;
    expected_expression = 6;
    expected_statement = 7;
    unexpected_token = 8;
    duplicate_function = 9;
    missing_main = 10;
    duplicate_parameter = 11;
    duplicate_local = 12;
    undeclared_variable = 13;
    undefined_function = 14;
    argument_count_mismatch = 15;
    void_value = 16;
    int_return_value_missing = 17;
    void_return_value = 18;
    constant_divide_by_zero = 19;
}
