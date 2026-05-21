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
}
