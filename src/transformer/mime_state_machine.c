#include <string.h>
#include "filter_list.h"
#include "mime_parser.h"
#include "validations.h"
#include "parser_types.h"
#include "mime_state_machine.h"
#include "shared.h"
#include "buffer_utils.h"
#include "parser_utils.h"

character_token_type get_current_token(character_token_type character_token, char c) {
	if((character_token == REGULAR || character_token == FOLDING) && c == '\r'){
		return CR;
	}
	if(character_token == CR && c == '\n'){
		return CRLF;
	}
	if(character_token == CRLF && c == '\r'){
		return CRLFCR;
	}
	if(character_token == CRLFCR && c == '\n'){
		return CRLFCRLF;
	}
	if((character_token == CRLF || character_token == FOLDING) && is_white_space(c)){
		return FOLDING;
	}
	if(character_token == CRLF && !is_white_space(c)){
		return NEW_LINE;
	}
	return REGULAR;
}

void handle_maybe_message(bool is_message) {
	if (is_message) {
		modify_state(HEADER_NAME);
	} else {
		modify_state(CONTENT_TYPE_BODY);
	}
}

int handle_header_name(char current_character, buffer_type * helper_buffer) {
	if (current_character == ':') {
		bool is_content_type = strcicmp(helper_buffer->buffer, "Content-Type") == 0;
		clear_buffer(helper_buffer);
		if (is_content_type) {
			modify_state(CONTENT_TYPE_TYPE);
		} else {
			modify_state(HEADER_VALUE);
		}
		return 0;
	}
	if(get_character_token() == CRLFCR){
		modify_character_token(REGULAR);
		modify_state(CONTENT_TYPE_BODY);
		clear_buffer(helper_buffer);
		return 0;
	}
	if (is_delimiter(current_character)) {
		modify_state(POSSIBLE_ERROR);
		return 1;
	}

	return 0;
}

int handle_header_value(char current_character, buffer_type * helper_buffer, FILE * transformed_mail, content_type_and_subtype current_content_type) {
	if(get_character_token() == NEW_LINE){
		modify_character_token(REGULAR);
		modify_state(HEADER_NAME);
		clear_buffer(helper_buffer);
		write_to_helper_buffer(current_character, helper_buffer);
		return state_transition(current_character, transformed_mail);
	}
	if (get_character_token() == CRLFCRLF) {
		bool is_message = strcicmp(current_content_type.type, "message") == 0;
		clear_buffer(helper_buffer);
		modify_character_token(REGULAR);
		handle_maybe_message(is_message);
		return 0;
	}

	return 0;
}

int handle_attribute_name(char current_character, buffer_type * helper_buffer) {
	if(current_character == '='){
		bool is_boundary = strcicmp(helper_buffer->buffer, "boundary") == 0;
		clear_buffer(helper_buffer);
		if(is_boundary) {
			modify_state(BOUNDARY);
		} else {
			modify_state(ATTRIBUTE_VALUE);
		}
		return 0;
	}

	if (is_delimiter(current_character)) {
		modify_state(POSSIBLE_ERROR);
		return 1;
	}

	return 0;
}
