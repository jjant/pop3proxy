#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "parser_utils.h"
#include "parser_types.h"
#include "blacklist.h"
#include "buffer_utils.h"

static cTypeNSubType * create_blacklist_element(char * type);
static char * allocate_for_string(char * str);
static bool is_type_equal(char * type1, char * type2);
static bool is_subtype_equal(char * subtype1, char * subtype2);
static bool is_subtype_wildcard(char * subtype);
static bool does_content_type_match(cTypeNSubType * content_type1, cTypeNSubType * content_type2);

void free_blacklist(cTypeNSubType * blacklist[]) {
	for(int k = 0; blacklist[k] != NULL; k++) {
		free(blacklist[k]->type);
		free(blacklist[k]->subtype);
   		free(blacklist[k]);
   	}
}

static bool is_type_equal(char * type1, char * type2) {
	return strcicmp(type1, type2) == 0;
}

static bool is_subtype_equal(char * subtype1, char * subtype2) {
	return strcicmp(subtype1, subtype2) == 0;
}

static bool is_subtype_wildcard(char * subtype) {
	return subtype != NULL && subtype[0] == '*';
}

static bool does_content_type_match(cTypeNSubType * content_type1, cTypeNSubType * content_type2) {
	bool does_type_match = is_type_equal(content_type1->type, content_type2->type);
	bool does_subtype_match = is_subtype_wildcard(content_type2->subtype) || is_subtype_equal(content_type1->subtype, content_type2->subtype);

	return does_type_match && does_subtype_match;
}

bool is_in_blacklist(cTypeNSubType * blacklist[], cTypeNSubType* content) {
	for(int k = 0; blacklist[k] != NULL; k++) {
 		if (does_content_type_match(content, blacklist[k])) return true;
 	}

 	return false;
}

static cTypeNSubType * create_blacklist_element(char * type) {
	cTypeNSubType * const blacklist_element = malloc(sizeof(cTypeNSubType));

	blacklist_element->type = allocate_for_string(type);
	strcpy(blacklist_element->type, type);

	return blacklist_element;
}

static char * allocate_for_string(char * str) {
	return (char *)malloc((strlen(str) + 1) * sizeof(char));
}

static char * get_type() { return ""; }

static char * get_subtype() { return ""; }
	
void populate_blacklist(cTypeNSubType * blacklist[], char * items, char buffer[]) {
	int i = 0;

	while(items[0] != '\0') {
		items = copy_to_buffer(items, buffer);

		if(items[0] == '/') {
			blacklist[i] = create_blacklist_element(buffer);
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
