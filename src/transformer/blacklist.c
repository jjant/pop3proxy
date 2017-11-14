#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "parser_utils.h"
#include "parser_types.h"
#include "blacklist.h"
#include "buffer_utils.h"

static cTypeNSubType * create_blacklist_element(char * type, char * subtype);
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

static cTypeNSubType * create_blacklist_element(char * type, char * subtype) {
	cTypeNSubType * const blacklist_element = malloc(sizeof(cTypeNSubType));

	blacklist_element->type = allocate_for_string(type);
	blacklist_element->subtype = allocate_for_string(subtype);
	strcpy(blacklist_element->type, type);
	strcpy(blacklist_element->subtype, subtype);

	return blacklist_element;
}

static char * allocate_for_string(char * str) {
	return (char *)malloc((strlen(str) + 1) * sizeof(char));
}

static char * get_type(char * token, char ** save_ptr) {
	char * type = strtok_r(token, "/", save_ptr);

	return type;
}

static char * get_subtype(char ** save_ptr) {
	char * subtype = strtok_r(NULL, "/", save_ptr);

	return subtype;
}

void populate_blacklist(cTypeNSubType * blacklist[], char * items, char buffer[]) {
	int i = 0;
	char * save_ptr1;
	char * token;
	char * items_copy = allocate_for_string(items);
	strcpy(items_copy, items);

	token = strtok_r(items_copy, ",", &save_ptr1);

	if (token == NULL) {
		blacklist[i] = NULL;
		return;
	}

	do {
		char * save_ptr2 = NULL;
		char * type = get_type(token, &save_ptr2);
		char * subtype = get_subtype(&save_ptr2);

		blacklist[i] = create_blacklist_element(type, subtype);
	} while((token = strtok_r(NULL, ",", &save_ptr1)) != NULL);

	blacklist[i] = NULL;
}
