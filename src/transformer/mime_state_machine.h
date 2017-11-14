#ifndef MIME_STATE_MACHINE_H
#define MIME_STATE_MACHINE_H

#include <stdio.h>
#include "parser_types.h"

/* Returns the current character token */
character_token_type get_current_token(character_token_type character_token, char c);
/* Handles state change when a header might be message */
void handle_maybe_message(bool is_message);

/* Handles state machine case for HEADER_NAME state */
int handle_header_name(char current_character, buffer_type * helper_buffer);
/* Handles state machine case for HEADER_VALUE state */
int handle_header_value(char current_character, buffer_type * helper_buffer, FILE * transformed_mail, content_type_and_subtype current_content_type);
/* Handles state machine case for ATTRIBUTE_NAME state */
int handle_attribute_name(char current_character, buffer_type * helper_buffer);

#endif
