#ifndef MIME_STATE_MACHINE_H
#define MIME_STATE_MACHINE_H

#include <stdio.h>
#include "parser_types.h"

/* Returns the current character token */
char_types get_current_token(char_types character_token, char c);
/* Handles state change when a header might be message */
void handle_maybe_message(state_type * state, bool is_message);
int state_transition(state_type * state, char c, FILE * transformed_mail);

#endif
