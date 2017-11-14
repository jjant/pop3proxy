#ifndef VALIDATIONS_H
#define VALIDATIONS_H

#include <stdbool.h>
#include "parser_types.h"

/* Check if an email is valid */
bool is_email_invalid(content_type_stack_type * content_type_stack);
/* Check if a character is a delimiter */
bool is_delimiter(char c);
/* Check if a character is whitespace */
bool is_white_space(char c);
/* Check if a character is whitespace or a delimiter */
bool is_white_space_or_separator(char c, character_token_type character_token);
/* Check if a character is a delimiter or boundary */
bool is_limit_or_boundary(char c);
/* Check if a character is a CR or LF character */
bool is_cr_or_lf(char c);
bool should_write(bool erasing, state_type state);
/* Check if a '(' represents a comment start */
bool is_comment_start(char current_character, int in_comment);
/* Check if a '(' represents a comment end */
bool is_comment_end(char current_character, int in_comment);

#endif
