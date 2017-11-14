#include <string.h>
#include <stdbool.h>
#include "parser_types.h"
#include "boundary_utils.h"

/* Checks if a string starts with two dashes ('--'). */
static bool matches_first_two_dashes(char * str);

static bool matches_first_two_dashes(char * str) {
	return !(strlen(str) >= 2 && str[0] != '-' && str[1] != '-');
}

boundary_type str_check_boundary(char * str, char * boundary) {
	if (!matches_first_two_dashes(str)) {
		return NOT_BOUNDARY;
	}
	// Skip first two '-' characters.
	str = str + 2;

	int index = 0;

	for(; str[index] != '\0' && boundary[index] != '\0'; index++) {
		if(str[index] != boundary[index]) return NOT_BOUNDARY;
	}

	/* If boundary is consumed, and str ends in two dashes ('--'), it's a finishing boundary. */
	if (boundary[index] == 0 &&
		 (str[index] != 0 && str[index + 1] != 0)
		 && (str[index] == '-' && str[index + 1] == '-')) {
		return FINISH;
	}

	/* If it doesn't end up in two dashes, it's just a separator. */
	if (boundary[index] == 0) {
		return SEPARATOR;
	}

	/* Else, it's not a boundary */
	return NOT_BOUNDARY;
}
