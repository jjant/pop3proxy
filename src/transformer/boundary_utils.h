#ifndef BOUNDARY_UTILS_H
#define BOUNDARY_UTILS_H

#include "parser_types.h"

/* Check if a string is a boundary. If it is, it returns if it is a finishing boundary or a separating one. */
boundary_type str_check_boundary(char * str, char * boundary);

#endif
