#include "validations.h"
#include "parser_types.h"
#include "mime_state_machine.h"
#include "shared.h"

char_types get_current_state_char(char_types character_token, char c) {
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
