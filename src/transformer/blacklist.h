#ifndef BLACKLIST_H
#define BLACKLIST_H

#include "parser_types.h"

/* TODO: Document */
void free_blacklist(cTypeNSubType * blacklist[]);
/* Verifica si un contenido dado debe ser censurado o no. */
int is_in_blacklist(cTypeNSubType * blacklist[], cTypeNSubType* content, Buffer comparison_buffer);
void populate_blacklist(cTypeNSubType * blacklist[], char * items, char buffer[]);

#endif
