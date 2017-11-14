#include "parser_types.h"
#include "boundary_utils.h"

boundary_type check_boundary(char * str, char * boundary) {
	if(str[0] != 0 && str[1] != 0 && str[0] != '-' && str[1] != '-') return NOT_BOUNDARY;

	str = str + 2;
	int i = 0;

	for(; str[i] != 0 && boundary[i] != 0; i++) {
		if(str[i] != boundary[i]) return NOT_BOUNDARY;
	}

	if(boundary[i] == 0 && str[i] != 0 && str[i+1] != 0 && str[i] == '-' && str[i+1] == '-') {
		return FINISH;
	}

	if (boundary[i] == 0) {
		return SEPARATOR;
	}

	return NOT_BOUNDARY;
}
