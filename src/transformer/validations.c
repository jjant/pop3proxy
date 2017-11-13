#include <stdlib.h>
#include <stdbool.h>
#include "parser_types.h"
#include "validations.h"

bool is_email_invalid(cTypeStack * content_type_stack) {
	return content_type_stack != NULL && (content_type_stack->type != DISCRETE || content_type_stack->prev != NULL);
}
