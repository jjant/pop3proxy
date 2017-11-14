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
static buffer_type print_buffer;
static buffer_type comparison_buffer;
static state_type state;
static char_types character_token;
static content_type_stack_type * content_type_stack;
static filter_list_type filter_list;
static content_type_and_subtype current_content_type;
static char * filter_replacement_message;
static bool erasing = false;

static void initialize_global_variables();
static void error(void);

static void error() {
	state = POSSIBLE_ERROR;
}

int state_transition(char c, FILE * transformed_mail);


static void initialize_global_variables(char * filter_medias, char * filter_message) {
	filter_list[0] = NULL;
	if (filter_medias != NULL) initialize_filter_list(filter_list, filter_medias);
	filter_replacement_message = filter_message;

	state = HEADER_NAME;
	character_token = COMMON;
	current_content_type.type = NULL;
	current_content_type.subtype = NULL;
	content_type_stack = malloc(sizeof(content_type_stack_type));
	content_type_stack->prev = NULL;
	content_type_stack->next = NULL;
	content_type_stack->type = NO_CONTENT;
	content_type_stack->boundary = NULL;
	print_buffer.index = 0;
	comparison_buffer.index = 0;
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
	int number_read;
 	int is_comment = 1;
	int i = 0;
	char current_character;

	char * retrieved_mail_file_path  = get_retrieved_mail_file_path(client_number);
  char * transformed_mail_file_path = get_transformed_mail_file_path(client_number);
	FILE * retrieved_mail   = fopen(retrieved_mail_file_path, "r");
	FILE * transformed_mail = fopen(transformed_mail_file_path, "a");

	initialize_global_variables(filter_medias, filter_message);

	// Eat first characters which correspond to pop3 +OK/-ERR stuff
	fread(aux, CHARACTER_SIZE, index_from_which_to_read_file, retrieved_mail);

	#define READ_COUNT BUFSIZE - 1

  while ((number_read = fread(aux, CHARACTER_SIZE, READ_COUNT, retrieved_mail)) > 0) {
    for (int index = 0; index < number_read && i < BUFFER_SIZE; index++) {
   		current_character = aux[index];

			if(current_character == '(' && is_comment == 1) {
				is_comment = 0;
	   	} else if(current_character == ')' && is_comment == 0) {
	   		is_comment = 1;
	   	}

	   	if (should_write(erasing, state)) {
				write_to_out_buff(current_character, &print_buffer, transformed_mail);
	   	}

	   	character_token = get_current_token(character_token, current_character);

	   	if(is_comment == 0) continue;

	   	if(!is_white_space_or_separator(current_character, character_token) || (state == BOUNDARY && !is_limit_or_boundary(current_character)) || (state == CONTENT_TYPE_BODY && !is_cr_or_lf(current_character))) {
				if ((character_token != NEW_LINE && character_token != FOLD) || (state == CONTENT_TYPE_BODY && character_token == NEW_LINE)) {
	   			write_to_comparison_buffer(current_character, &comparison_buffer);
	   		}
	   	}
			state_transition(current_character, transformed_mail);
		}
 	}

	const char * buffer = print_buffer.buff;
	fwrite(buffer, CHARACTER_SIZE, strlen(buffer), transformed_mail);

	print_buffer.index = 0;
	free_filter_list(filter_list);
	free(current_content_type.type);
	free(current_content_type.subtype);

  fclose(retrieved_mail);
  fclose(transformed_mail);

	if (is_email_invalid(content_type_stack)) {
		handle_error(client_number);
	}

 	return 0;
}

int modify_state(state_type new_state) {
	state = new_state;
}


int handle_header_value(char current_character, FILE * transformed_mail) {
	if(character_token == NEW_LINE){
		character_token = COMMON;
		state = HEADER_NAME;
		clear_buffer(&comparison_buffer);
		write_to_comparison_buffer(current_character, &comparison_buffer);
		return state_transition(current_character, transformed_mail);
	}
	if (character_token == CRLFCRLF) {
		bool is_message = strcicmp(current_content_type.type, "message") == 0;
		clear_buffer(&comparison_buffer);
		character_token = COMMON;
		handle_maybe_message(&state, is_message);
		return 0;
	}

	return 0;
}

int handle_content_type_type(char current_character) {
	if(current_character == '/') {
		if(current_content_type.type != NULL) {
			free(current_content_type.type);
		}
		current_content_type.type = allocate_for_string(comparison_buffer.buff);
		strcpy(current_content_type.type, comparison_buffer.buff);
		state = CONTENT_TYPE_SUBTYPE;
		clear_buffer(&comparison_buffer);
		return 0;
	}
	if (is_delimiter(current_character)){
		error();
		return 1;
	}

	return 0;
}

int handle_content_type_subtype(char current_character, FILE * transformed_mail) {
	if(current_character == ';' || character_token == NEW_LINE || character_token == CRLFCRLF){
		if(current_content_type.subtype != NULL) {
			free(current_content_type.subtype);
		}
		current_content_type.subtype = (char*)malloc((strlen(comparison_buffer.buff) + 1) * sizeof(char));
		strcpy(current_content_type.subtype, comparison_buffer.buff);
		bool is_content_in_filter_list = is_in_filter_list(filter_list, &current_content_type);
		clear_buffer(&comparison_buffer);
		if (is_content_in_filter_list) {
			write_str_to_out_buff(" text/plain\r\n\r\n", &print_buffer, transformed_mail);
			write_str_to_out_buff(filter_replacement_message, &print_buffer, transformed_mail);
			write_str_to_out_buff("\r\n", &print_buffer, transformed_mail);
			content_type_stack->type = DISCRETE;
			erasing = true;
			state = CONTENT_TYPE_BODY;
			return 0;
		}
		bool is_multipart = strcicmp(current_content_type.type,"multipart") == 0;
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
		write_str_to_out_buff(current_content_type.type, &print_buffer, transformed_mail);
		write_str_to_out_buff("/", &print_buffer, transformed_mail);
		write_str_to_out_buff(current_content_type.subtype, &print_buffer, transformed_mail);

		if(current_character == ';') {
			state = ATTRIBUTE_NAME;
			clear_buffer(&comparison_buffer);
			write_str_to_out_buff(";", &print_buffer, transformed_mail);
			return 0;
		}
		if(character_token == NEW_LINE){
			character_token = COMMON;
			state = HEADER_NAME;
			clear_buffer(&comparison_buffer);
			write_str_to_out_buff("\r\n", &print_buffer, transformed_mail);
			write_to_out_buff(current_character, &print_buffer, transformed_mail);
			write_to_comparison_buffer(current_character, &comparison_buffer);
			return state_transition(current_character, transformed_mail);
		}
		if (character_token == CRLFCRLF) {
			character_token = COMMON;
			write_str_to_out_buff("\r\n\r\n", &print_buffer, transformed_mail);
			bool is_message = strcicmp(current_content_type.type,"message") == 0;
			clear_buffer(&comparison_buffer);
			handle_maybe_message(&state, is_message);
			return 0;
		}
	}

	return 0;
}

int handle_attribute_name(char current_character) {
	if(current_character == '='){
		bool is_boundary = strcicmp(comparison_buffer.buff, "boundary") == 0;
		clear_buffer(&comparison_buffer);
		if(is_boundary) {
			state = BOUNDARY;
		} else {
			state = ATTRIBUTE_VALUE;
		}
		return 0;
	}

	if (is_delimiter(current_character)) {
		error();
		return 1;
	}

	return 0;
}

int handle_attribute_value(char current_character, FILE * transformed_mail) {
	if(current_character == ';') {
		state = ATTRIBUTE_NAME;
		clear_buffer(&comparison_buffer);
		return 0;
	}
	if(character_token == NEW_LINE){
		character_token = COMMON;
		state = HEADER_NAME;
		clear_buffer(&comparison_buffer);
		write_to_comparison_buffer(current_character, &comparison_buffer);
		return state_transition(current_character, transformed_mail);
	}
	if (character_token == CRLFCRLF) {
		bool is_message = strcicmp(current_content_type.type,"message") == 0;
		clear_buffer(&comparison_buffer);
		character_token = COMMON;
		handle_maybe_message(&state, is_message);
		return 0;
	}

	return 0;
}

int handle_boundary(char current_character, FILE * transformed_mail) {
	if(current_character == ';' || character_token == NEW_LINE || character_token == CRLFCRLF) {
		content_type_stack->boundary = malloc((strlen(comparison_buffer.buff) + 1) * sizeof(char));
		strcpy(content_type_stack->boundary, comparison_buffer.buff);
		clear_buffer(&comparison_buffer);
		if(current_character == ';') {
			state = ATTRIBUTE_NAME;
			clear_buffer(&comparison_buffer);
			return 0;
		}
		if(character_token == NEW_LINE){
			character_token = COMMON;
			state = HEADER_NAME;
			clear_buffer(&comparison_buffer);
			write_to_comparison_buffer(current_character, &comparison_buffer);
			return state_transition(current_character, transformed_mail);
		}
		if (character_token == CRLFCRLF) {
			bool is_message = strcicmp(current_content_type.type,"message") == 0;
			clear_buffer(&comparison_buffer);
			character_token = COMMON;
			handle_maybe_message(&state, is_message);
			return 0;
		}
	}

	return 0;
}

int handle_content_type_body(char current_character, FILE * transformed_mail) {
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
					state = POSSIBLE_ERROR;
				}
				erasing = false;
			}
		}
		clear_buffer(&comparison_buffer);
		if(state == HEADER_NAME){
			character_token = COMMON;
		}
	}

	return 0;
}

int state_transition(char current_character, FILE * transformed_mail) {
	switch(state) {
		case POSSIBLE_ERROR:
			return 0;
		case HEADER_NAME:
			return handle_header_name(current_character, &comparison_buffer);
		case HEADER_VALUE:
			return handle_header_value(current_character, transformed_mail);
		case CONTENT_TYPE_TYPE:
			return handle_content_type_type(current_character);
		case CONTENT_TYPE_SUBTYPE:
			return handle_content_type_subtype(current_character, transformed_mail);
		case ATTRIBUTE_NAME:
			return handle_attribute_name(current_character);
		case ATTRIBUTE_VALUE:
			return handle_attribute_value(current_character, transformed_mail);
		case BOUNDARY:
			return handle_boundary(current_character, transformed_mail);
		case CONTENT_TYPE_BODY:
			return handle_content_type_body(current_character, transformed_mail);
		default:
			return 0;
	}
}
