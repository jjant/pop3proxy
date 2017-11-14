#include "parser_types.h"
#include "mime_state_machine.h"
#include "shared.h"

char_types get_current_state_char(char_types read_chars, char c) {
	if((read_chars == COMMON || read_chars == FOLD) && c == '\r'){
		return CR;
	}
	if(read_chars == CR && c == '\n'){
		return CRLF;
	}
	if(read_chars == CRLF && c == '\r'){
		return CRLFCR;
	}
	if(read_chars == CRLFCR && c == '\n'){
		return CRLFCRLF;
	}
	if((read_chars == CRLF || read_chars == FOLD) && WHITESPACE(c)){
		return FOLD;
	}
	if(read_chars == CRLF && !WHITESPACE(c)){
		return NEW_LINE;
	}
	return COMMON;
}
