#ifndef filter_list_H
#define filter_list_H

#include <stdbool.h>
#include "parser_types.h"

/* TODO: Document */
void free_filter_list(content_type_and_subtype * filter_list[]);
/* TODO: Documentar */
bool is_in_filter_list(content_type_and_subtype * filter_list[], content_type_and_subtype* content);
/* TODO: Documentar, cambiar nombre */
void populate_filter_list(content_type_and_subtype * filter_list[], char * items);

#endif
