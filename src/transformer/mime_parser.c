#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "parser_utils.h"
#include "mime_parser.h"
#include "buffer_utils.h"
#include "shared.h"
#include "filter_list.h"
#include "parser_types.h"
#include "boundary_utils.h"
#include "validations.h"
#include "mime_state_machine.h"

#define BUFSIZE 1024

static char aux[BUFSIZE] = { 0 };
static Buffer print_buffer;
static Buffer comparison_buffer;
static states state;
static char_types character_token;
static content_type_stack_type * content_type_stack;
static filter_list_type filter_list;
static content_type_and_subtype actual_content;
static char * substitute_text;
static bool erasing = false;

static int state_transition(char c, FILE * transformed_mail);
static void error(void);

static void error(void) {
	state = TRANSPARENT;
}

static void handle_error(char * number) {
	const char * error_text = "-ERR Connection error or malformed mail\r\n";
	const char * file_path = get_transformed_mail_file_path(number);

	remove(file_path);

  FILE * transformed_mail = fopen(file_path, "w");
	fwrite(error_text, CHARACTER_SIZE, strlen(error_text), transformed_mail);
	fclose(transformed_mail);
}

int mime_parser(char * filter_medias, char * filter_message, char * client_number, size_t index_from_which_to_read_file) {
	int i = 0;
	char current_character;
	state = HEADER_NAME;
	character_token = COMMON;

	char * retrieved_mail_file_path  = get_retrieved_mail_file_path(client_number);
  char * transformed_mail_file_path = get_transformed_mail_file_path(client_number);
	FILE * retrieved_mail   = fopen(retrieved_mail_file_path, "r");
	FILE * transformed_mail = fopen(transformed_mail_file_path, "a");

 	filter_list[0] = NULL;

	if (filter_medias != NULL) populate_filter_list(filter_list, filter_medias);

	substitute_text = filter_message;

	actual_content.type = NULL;
	actual_content.subtype = NULL;
 	content_type_stack = malloc(sizeof(content_type_stack_type));
 	content_type_stack->prev = content_type_stack->next = NULL;
 	content_type_stack->type = NO_CONTENT;
	content_type_stack->boundary = NULL;
 	print_buffer.index = 0;
	comparison_buffer.index = 0;
 	int number_read;
 	int is_comment = 1;

	// Eat first characters which correspond to pop3 +OK/-ERR stuff
	fread(aux, CHARACTER_SIZE, index_from_which_to_read_file, retrieved_mail);

	#define READ_COUNT BUFSIZE - 1

  while ((number_read = fread(aux, CHARACTER_SIZE, READ_COUNT, retrieved_mail)) > 0) {
    for (int index = 0; index < number_read && i < BUFFLEN; index++) {
   		current_character = aux[index];

			if(current_character == '(' && is_comment == 1) {
				is_comment = 0;
	   	} else if(current_character == ')' && is_comment == 0) {
	   		is_comment = 1;
	   	}

	   	if (should_write(erasing, state)) {
				write_to_out_buff(current_character, &print_buffer, transformed_mail);
	   	}

	   	character_token = get_current_state_char(character_token, current_character);

	   	if(is_comment == 0) continue;

	   	if(!is_white_space_or_separator(current_character, character_token) || (state == BOUNDARY && !is_limit_or_boundary(current_character)) || (state == CONTENT_DATA && !is_cr_or_lf(current_character))) {
				if ((character_token != NEW_LINE && character_token != FOLD) || (state == CONTENT_DATA && character_token == NEW_LINE)) {
	   			write_to_comp_buff(current_character, &comparison_buffer);
	   		}
	   	}
			state_transition(current_character, transformed_mail);
		}
 	}

	const char * buffer = print_buffer.buff;
	fwrite(buffer, CHARACTER_SIZE, strlen(buffer), transformed_mail);

	print_buffer.index = 0;
	free_filter_list(filter_list);
	free(actual_content.type);
	free(actual_content.subtype);

  fclose(retrieved_mail);
  fclose(transformed_mail);

	if (is_email_invalid(content_type_stack)) {
		handle_error(client_number);
	}

 	return 0;
}

static int state_transition(char c, FILE * transformed_mail) {
	switch(state) {
		case TRANSPARENT:
			return 0;
		case HEADER_NAME:
			if(c == ':'){
				bool is_content_type = strcicmp(comparison_buffer.buff, "Content-Type") == 0;
				clear_buffer(&comparison_buffer);
	   		if (is_content_type) {
					state = CTYPE_DATA;
				} else {
					state = HEADER_DATA;
				}
				return 0;
		   	}
		   	if(character_token == CRLFCR){
		   		character_token = COMMON;
		   		state = CONTENT_DATA;
					clear_buffer(&comparison_buffer);
		   		return 0;
		   	}
		   	if (is_delimiter(c)){
		   		error();
		   		return 1;
		   	}
			break;
		case HEADER_DATA:
			if(character_token == NEW_LINE){
				character_token = COMMON;
				state = HEADER_NAME;
				clear_buffer(&comparison_buffer);
  			write_to_comp_buff(c, &comparison_buffer);
				return state_transition(c, transformed_mail);
			}
			if (character_token == CRLFCRLF) {
				bool is_message = strcicmp(actual_content.type, "message") == 0;
				clear_buffer(&comparison_buffer);
				character_token = COMMON;
				if (is_message) {
					state = HEADER_NAME;
				} else {
					state = CONTENT_DATA;
				}
	   		return 0;
			}
			break;
		case CTYPE_DATA:
			if(c == '/') {
				if(actual_content.type != NULL) {
					free(actual_content.type);
				}
				actual_content.type = (char*)malloc((strlen(comparison_buffer.buff) + 1) * sizeof(char));
				strcpy(actual_content.type, comparison_buffer.buff);
				state = CSUBTYPE_DATA;
				clear_buffer(&comparison_buffer);
				return 0;
			}
			if (is_delimiter(c)){
		   		error();
		   		return 1;
		   	}
			break;
		case CSUBTYPE_DATA:
			if(c == ';' || character_token == NEW_LINE || character_token == CRLFCRLF){
				if(actual_content.subtype != NULL) {
					free(actual_content.subtype);
				}
				actual_content.subtype = (char*)malloc((strlen(comparison_buffer.buff) + 1) * sizeof(char));
				strcpy(actual_content.subtype, comparison_buffer.buff);
				bool is_content_in_filter_list = is_in_filter_list(filter_list, &actual_content);
				clear_buffer(&comparison_buffer);
				if (is_content_in_filter_list) {
					write_str_to_out_buff(" text/plain\r\n\r\n", &print_buffer, transformed_mail);
					write_str_to_out_buff(substitute_text, &print_buffer, transformed_mail);
					write_str_to_out_buff("\r\n", &print_buffer, transformed_mail);
					content_type_stack->type = DISCRETE;
					erasing = true;
					state = CONTENT_DATA;
					return 0;
				}
				bool is_multipart = strcicmp(actual_content.type,"multipart") == 0;
				clear_buffer(&comparison_buffer);
				if (is_multipart) {
					content_type_stack->type = COMPOSITE;
					content_type_stack->next = malloc(sizeof(*(content_type_stack->next)));
					content_type_stack->next->prev = content_type_stack;
					content_type_stack->next->next = NULL;
					content_type_stack->boundary = NULL;
					content_type_stack->next->type = NO_CONTENT;
				} else {
					content_type_stack->type = DISCRETE;
				}
				write_str_to_out_buff(actual_content.type, &print_buffer, transformed_mail);
				write_str_to_out_buff("/", &print_buffer, transformed_mail);
				write_str_to_out_buff(actual_content.subtype, &print_buffer, transformed_mail);

				if(c == ';') {
					state = ATTR_NAME;
					clear_buffer(&comparison_buffer);
					write_str_to_out_buff(";", &print_buffer, transformed_mail);
					return 0;
				}
				if(character_token == NEW_LINE){
					character_token = COMMON;
					state = HEADER_NAME;
					clear_buffer(&comparison_buffer);
					write_str_to_out_buff("\r\n", &print_buffer, transformed_mail);
					write_to_out_buff(c, &print_buffer, transformed_mail);
					write_to_comp_buff(c, &comparison_buffer);
					return state_transition(c, transformed_mail);
				}
				if (character_token == CRLFCRLF) {
					character_token = COMMON;
		   		write_str_to_out_buff("\r\n\r\n", &print_buffer, transformed_mail);
					bool is_message = strcicmp(actual_content.type,"message") == 0;
					clear_buffer(&comparison_buffer);
					if (is_message) {
						state = HEADER_NAME;
					} else {
						state = CONTENT_DATA;
					}
		   		return 0;
				}
			}
			break;
		case ATTR_NAME:
			if(c == '='){
				bool is_boundary = strcicmp(comparison_buffer.buff,"boundary") == 0;
				clear_buffer(&comparison_buffer);
				if(is_boundary) {
					state = BOUNDARY;
				} else {
					state = ATTR_DATA;
				}
				return 0;
			}
			if (is_delimiter(c)){
		   		error();
		   		return 1;
		   	}
			break;
		case ATTR_DATA:
			if(c == ';') {
				state = ATTR_NAME;
				clear_buffer(&comparison_buffer);
				return 0;
			}
			if(character_token == NEW_LINE){
				character_token = COMMON;
				state = HEADER_NAME;
				clear_buffer(&comparison_buffer);
				write_to_comp_buff(c, &comparison_buffer);
				return state_transition(c, transformed_mail);
			}
			if (character_token == CRLFCRLF) {
				bool is_message = strcicmp(actual_content.type,"message") == 0;
				clear_buffer(&comparison_buffer);
				character_token = COMMON;
				if (is_message) {
  				state = HEADER_NAME;
  			} else {
  				state = CONTENT_DATA;
  			}
	   		return 0;
			}
			break;
		case BOUNDARY:
			if(c == ';' || character_token == NEW_LINE || character_token == CRLFCRLF){
 			  content_type_stack->boundary = malloc((strlen(comparison_buffer.buff) + 1) * sizeof(char));
				strcpy(content_type_stack->boundary, comparison_buffer.buff);
				clear_buffer(&comparison_buffer);
				if(c == ';') {
					state = ATTR_NAME;
					clear_buffer(&comparison_buffer);
					return 0;
				}
				if(character_token == NEW_LINE){
					character_token = COMMON;
					state = HEADER_NAME;
					clear_buffer(&comparison_buffer);
					write_to_comp_buff(c, &comparison_buffer);
					return state_transition(c, transformed_mail);
				}
				if (character_token == CRLFCRLF) {
					bool is_message = strcicmp(actual_content.type,"message") == 0;
					clear_buffer(&comparison_buffer);
					character_token = COMMON;
					if (is_message) {
						state = HEADER_NAME;
					} else {
						state = CONTENT_DATA;
					}
		   		return 0;
				}
			}
			break;
		case CONTENT_DATA:
			if(character_token == CRLF || character_token == CRLFCRLF) {
				if(content_type_stack->type == COMPOSITE && check_boundary(comparison_buffer.buff, content_type_stack->boundary) == SEPARATOR) {
					if(erasing == true){
						write_str_to_out_buff("--", &print_buffer, transformed_mail);
						write_str_to_out_buff(content_type_stack->boundary, &print_buffer, transformed_mail);
					}
					content_type_stack = content_type_stack->next;
					content_type_stack->boundary = NULL;
					state = HEADER_NAME;
					erasing = false;
				} else if (content_type_stack->type != COMPOSITE && content_type_stack->prev != NULL) {
					boundary_type checked_bound = check_boundary(comparison_buffer.buff, content_type_stack->prev->boundary);
					if(checked_bound == SEPARATOR){
						if(erasing == true){
							write_str_to_out_buff("--", &print_buffer, transformed_mail);
							write_str_to_out_buff(content_type_stack->prev->boundary, &print_buffer, transformed_mail);
							write_str_to_out_buff("\r\n", &print_buffer, transformed_mail);
						}
						state = HEADER_NAME;
						erasing = false;
					} else if(checked_bound == FINISH) {
						if(erasing == true){
							write_str_to_out_buff("--", &print_buffer, transformed_mail);
							write_str_to_out_buff(content_type_stack->prev->boundary, &print_buffer, transformed_mail);
							write_str_to_out_buff("--\r\n", &print_buffer, transformed_mail);
						}
						free(content_type_stack->prev->boundary);
						content_type_stack->prev->boundary = NULL;
						content_type_stack_type* aux1 = content_type_stack;
						content_type_stack_type* aux2 = content_type_stack->prev;
						content_type_stack = content_type_stack->prev->prev;
						free(aux1);
						aux2->next = NULL;
						if(content_type_stack == NULL) {
							free(aux2);
							state = TRANSPARENT;
						}
						erasing = false;
					}
				}
				clear_buffer(&comparison_buffer);
  			if(state == HEADER_NAME){
  				character_token = COMMON;
  			}
			}
			break;
		default:
			break;
	}
	return 0;
}
