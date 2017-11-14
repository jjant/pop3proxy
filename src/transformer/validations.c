#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "parser_types.h"
#include "validations.h"
#include "shared.h"

bool is_email_invalid(content_type_stack_type * content_type_stack) {
	// return !(content_type_stack == NULL || (content_type_stack->type == SIMPLE && content_type_stack->prev == NULL));
	return false;
}

bool is_delimiter(char c) {
	return c == ';' || c == ':' || c == '=' || c == '/' || c == ',' || c == '\0';
}

bool is_white_space(char c) {
	return (c == ' ' || c == '	');
}

bool is_limit_or_boundary(char c) {
	return (c == '\r' || (c) == '\n' ||c == '"' || c == '\0');
}

bool is_cr_or_lf(char c) {
	return (c == '\r' || c == '\n');
}

bool is_white_space_or_separator(char c, character_token_type character_token) {
	return (is_delimiter(c) || is_white_space(c) || is_limit_or_boundary(c) || character_token == NEW_LINE);
}

bool should_write(bool erasing, state_type state) {
	return !(erasing == true || state == CONTENT_TYPE_TYPE || state == CONTENT_TYPE_SUBTYPE);
}

bool is_comment_start(char current_character, int in_comment) {
	return current_character == '(' && in_comment == 1;
}

bool is_comment_end(char current_character, int in_comment) {
	return current_character == ')' && in_comment == 0;
}
