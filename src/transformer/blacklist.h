#ifndef BLACKLIST_H
#define BLACKLIST_H

#include <stdbool.h>
#include "parser_types.h"

/* TODO: Document */
void free_blacklist(cTypeNSubType * blacklist[]);
/* TODO: Documentar */
bool is_in_blacklist(cTypeNSubType * blacklist[], cTypeNSubType* content);
/* TODO: Documentar, cambiar nombre */
void populate_blacklist(cTypeNSubType * blacklist[], char * items);

#endif
