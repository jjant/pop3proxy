#define _GNU_SOURCE 1

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "parser_utils.h"
#include "parser_types.h"
#include "filter_list.h"
#include "buffer_utils.h"

static content_type_and_subtype * create_filter_list_element(char * type, char * subtype);
static bool is_type_equal(char * type1, char * type2);
static bool is_subtype_equal(char * subtype1, char * subtype2);
static bool is_subtype_wildcard(char * subtype);
static bool does_content_type_match(content_type_and_subtype * content_type1, content_type_and_subtype * content_type2);

void free_filter_list(content_type_and_subtype * filter_list[]) {
	for(int k = 0; filter_list[k] != NULL; k++) {
		free(filter_list[k]->type);
		free(filter_list[k]->subtype);
   	free(filter_list[k]);
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

static bool does_content_type_match(content_type_and_subtype * content_type1, content_type_and_subtype * content_type2) {
	bool does_type_match = is_type_equal(content_type1->type, content_type2->type);
	bool does_subtype_match = is_subtype_wildcard(content_type2->subtype) || is_subtype_equal(content_type1->subtype, content_type2->subtype);

	return does_type_match && does_subtype_match;
}

bool is_in_filter_list(content_type_and_subtype * filter_list[], content_type_and_subtype* content) {
	for(int k = 0; filter_list[k] != NULL; k++) {
 		if (does_content_type_match(content, filter_list[k])) return true;
 	}

 	return false;
}

static content_type_and_subtype * create_filter_list_element(char * type, char * subtype) {
	content_type_and_subtype * const filter_list_element = malloc(sizeof(content_type_and_subtype));

	filter_list_element->type = allocate_for_string(type);
	filter_list_element->subtype = allocate_for_string(subtype);
	strcpy(filter_list_element->type, type);
	strcpy(filter_list_element->subtype, subtype);

	return filter_list_element;
}

static char * get_type(char * token, char ** save_ptr) {
	char * type = strtok_r(token, "/", save_ptr);

	return type;
}

static char * get_subtype(char ** save_ptr) {
	char * subtype = strtok_r(NULL, "/", save_ptr);

	return subtype;
}

void initialize_filter_list(content_type_and_subtype * filter_list[], char * items) {
	int i = 0;
	char * save_ptr1;
	char * token;
	char * items_copy = allocate_for_string(items);
	strcpy(items_copy, items);

	token = strtok_r(items_copy, ",", &save_ptr1);

	if (token == NULL) {
		filter_list[i] = NULL;
		return;
	}

	do {
		char * save_ptr2 = NULL;
		char * type = get_type(token, &save_ptr2);
		char * subtype = get_subtype(&save_ptr2);

		filter_list[i] = create_filter_list_element(type, subtype);
		i++;
	} while((token = strtok_r(NULL, ",", &save_ptr1)) != NULL);

	filter_list[i] = NULL;
}
