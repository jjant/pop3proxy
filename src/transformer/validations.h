#ifndef VALIDATIONS_H
#define VALIDATIONS_H

#include <stdbool.h>
#include "parser_types.h"

bool is_email_invalid(content_type_stack_type * content_type_stack);
bool is_delimiter(char c);
bool is_white_space(char c);
bool is_white_space_or_separator(char c, char_types character_token);
bool is_limit_or_boundary(char c);
bool is_cr_or_lf(char c);
bool should_write(bool erasing, state_type state);

#endif
