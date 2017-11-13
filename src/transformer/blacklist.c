#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "parser_utils.h"
#include "parser_types.h"
#include "blacklist.h"
#include "buffer_utils.h"

static cTypeNSubType * create_blacklist_element(char * type, bool isSubtype);
static char * allocate_for_string(char * str);

void free_blacklist(cTypeNSubType * blacklist[]) {
	for(int k = 0; blacklist[k] != NULL; k++) {
		free(blacklist[k]->type);
		free(blacklist[k]->subtype);
   		free(blacklist[k]);
   	}
}

int is_in_blacklist(cTypeNSubType * blacklist[], cTypeNSubType* content, Buffer comparison_buffer) {
	for(int k = 0; blacklist[k] != NULL; k++) {
   		if(strcicmp(content->type, blacklist[k]->type, &comparison_buffer) == 0){
   			if(blacklist[k]->subtype[0] == '*' || strcicmp(content->subtype, blacklist[k]->subtype, &comparison_buffer) == 0){
   				return 0;
   			}
   		}
   	}
   	return 1;
}

static cTypeNSubType * create_blacklist_element(char * type, bool isSubtype) {
	cTypeNSubType * const blacklist_element = malloc(sizeof(cTypeNSubType));

	if (isSubtype) {
		blacklist_element->subtype = allocate_for_string(type);
		strcpy(blacklist_element->subtype, type);
	} else {
		blacklist_element->type = allocate_for_string(type);
		strcpy(blacklist_element->type, type);
	}

	return blacklist_element;
}

static char * allocate_for_string(char * str) {
	return (char *)malloc((strlen(str) + 1) * sizeof(char));
}

/* Crea una lista con todos los Content Types que debe censurar */
void populate_blacklist(cTypeNSubType * blacklist[], char * items, char buffer[]) {
	int i = 0;

	while(items[0] != '\0') {
		items = copy_to_buffer(items, buffer);

		if(items[0] == '/') {
			blacklist[i] = create_blacklist_element(buffer, false);
			items++;
		} else {
			blacklist[i]->subtype = (char*)malloc((strlen(buffer) + 1) * sizeof(char));
			strcpy(blacklist[i]->subtype, buffer);
			if(items[0] == ',') {
				items++;
			}
			i++;
		}
	}

	blacklist[i] = NULL;
}
