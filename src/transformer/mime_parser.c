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
			state_transition(&state, current_character, transformed_mail);
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
