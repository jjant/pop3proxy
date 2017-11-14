#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "parser_types.h"
#include "validations.h"

bool is_email_invalid(content_type_stack_type * content_type_stack) {
	// return !(content_type_stack == NULL || (content_type_stack->type == DISCRETE && content_type_stack->prev == NULL));
	return false;
}
