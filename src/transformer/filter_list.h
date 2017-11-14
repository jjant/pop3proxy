#ifndef filter_list_H
#define filter_list_H

#include <stdbool.h>
#include "parser_types.h"

/* Deallocs memomry for the filter_list */
void free_filter_list(content_type_and_subtype * filter_list[]);
/* Check if a given type/subtype pair is in the filter_list */
bool is_in_filter_list(content_type_and_subtype * filter_list[], content_type_and_subtype* content);
/* Initializes the filter list by parsing the MIME types to be censored, passed via env var */
void initialize_filter_list(content_type_and_subtype * filter_list[], char * items);

#endif
