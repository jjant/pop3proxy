#include "validations.h"
#include "parser_types.h"
#include "mime_state_machine.h"
#include "shared.h"

char_types get_current_token(char_types character_token, char c) {
	if((character_token == COMMON || character_token == FOLD) && c == '\r'){
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
	if((character_token == CRLF || character_token == FOLD) && is_white_space(c)){
		return FOLD;
	}
	if(character_token == CRLF && !is_white_space(c)){
		return NEW_LINE;
	}
	return COMMON;
}

void handle_maybe_message(state_type * state, bool is_message) {
	if (is_message) {
		*state = HEADER_NAME;
	} else {
		*state = CONTENT_TYPE_BODY;
	}
}


int handle_header_name(char current_character, buffer_type * comparison_buffer) {
	if (current_character == ':') {
		bool is_content_type = strcicmp(comparison_buffer->buff, "Content-Type") == 0;
		clear_buffer(comparison_buffer);
		if (is_content_type) {
			state = CONTENT_TYPE_TYPE;
		} else {
			state = HEADER_VALUE;
		}
		return 0;
	}
	if(character_token == CRLFCR){
		character_token = COMMON;
		state = CONTENT_TYPE_BODY;
		clear_buffer(comparison_buffer);
		return 0;
	}
	if (is_delimiter(current_character)){
		error();
		return 1;
	}

	return 0;
}
