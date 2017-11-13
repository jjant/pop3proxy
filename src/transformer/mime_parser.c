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
#include "blacklist.h"
#include "parser_types.h"
#include "boundary_utils.h"
#include "validations.h"

Buffer printBuf;
Buffer compBuf;
char buff[BUFFLEN];
char bound[BUFFLEN];
char stdinBuf[BUFFLEN];
states state;
char_types read_chars;
cTypeStack* cType;
cTypeNSubType* blacklist[100];
cTypeNSubType actualContent;
char* substitute_text;
int erasing;

static int transition(char c, FILE * transformed_mail);
static void error();
static char_types get_current_state_char(char c);

static char_types get_current_state_char(char c) {
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

static void error() {
	state = TRANSPARENT;
}

#define BUFSIZE 1024

char aux[BUFSIZE] = { 0 };

static void handle_error(char * number) {
	const char * error_text = "-ERR Connection error or malformed mail\r\n";
	const char * file_path = get_transformed_mail_file_path(number);
	remove(file_path);
  FILE * transformed_mail = fopen(file_path, "w");
	fwrite(error_text, CHARACTER_SIZE, strlen(error_text), transformed_mail);
	fclose(transformed_mail);
}

int mime_parser(char * filter_medias, char * filter_message, char * client_number, size_t index_from_which_to_read_file) {
	char c;
	int i = 0;
	state = HEADER_NAME;
	read_chars = COMMON;

	char * retrieved_mail_file_path  = get_retrieved_mail_file_path(client_number);
  char * transformed_mail_file_path = get_transformed_mail_file_path(client_number);


	FILE * retrieved_mail   = fopen(retrieved_mail_file_path, "r");
	FILE * transformed_mail = fopen(transformed_mail_file_path, "a");

 	blacklist[0] = NULL;
 	// populate_blacklist("text/html,application/*");
	// substitute_text = "Content was deleted";

	if (filter_medias != NULL) populate_blacklist(blacklist, filter_medias, buff);

	substitute_text = filter_message;

	actualContent.type = NULL;
	actualContent.subtype = NULL;
 	cType = malloc(sizeof(cTypeStack));
 	cType->prev = cType->next = NULL;
 	cType->type = NO_CONTENT;
	cType->boundary = NULL;
 	printBuf.index = compBuf.index = 0;
 	int number_read;
 	int is_comment = 1;

	// Eat first characters which correspond to pop3 +ok/-err stuff
	fread(aux, CHARACTER_SIZE, index_from_which_to_read_file, retrieved_mail);

	#define READ_COUNT BUFSIZE - 1

  while ((number_read = fread(aux, CHARACTER_SIZE, READ_COUNT, retrieved_mail)) > 0) {
    for (int index = 0; index < number_read && i < BUFFLEN; index++) {
   		c = aux[index];


			if(c == '(' && is_comment == 1){
				is_comment = 0;
	   	} else if(c == ')' && is_comment == 0) {
	   		is_comment = 1;
	   	}

	   	if(!(erasing == ERASING || state == CTYPE_DATA || state == CSUBTYPE_DATA)) {
	   		write_to_out_buff(c, printBuf, transformed_mail);
	   	}

	   	read_chars = get_current_state_char(c);

	   	if(is_comment == 0) continue;

	   	if(!(LIMIT(c) ||
			 		 WHITESPACE(c) ||
					 LIMIT_BOUND(c) ||
					 read_chars == NEW_LINE) ||
				 (state == BOUNDARY && !LIMIT_BOUND(c)) ||
				 (state == CONTENT_DATA && !CRLF_CHAR(c))) {

				if((read_chars != NEW_LINE && read_chars != FOLD) || (state == CONTENT_DATA && read_chars == NEW_LINE)){
	   			write_to_comp_buff(c, compBuf);
	   		}

	   	}

			transition(c, transformed_mail);

		}
 	}



	const char * buffer = printBuf.buff;
	fwrite(buffer, CHARACTER_SIZE, strlen(buffer), transformed_mail);

	printBuf.index = 0;
	free_blacklist(blacklist);
	free(actualContent.type);
	free(actualContent.subtype);


  fclose(retrieved_mail);
  fclose(transformed_mail);

	if (is_email_invalid(cType)) {
		handle_error(client_number);
	}

 	return 0;
}

static int transition(char c, FILE * transformed_mail) {
	switch(state) {
		case TRANSPARENT:
			return 0;
		case HEADER_NAME:
			if(c == ':'){
				//printf("\nKAKAKA=%s\n", compBuf.buff);
		   		if(strcicmp(compBuf.buff, "Content-Type", &compBuf) == 0) {
					state = CTYPE_DATA;
				} else {
					state = HEADER_DATA;
				}
				return 0;
		   	}
		   	if(read_chars == CRLFCR){
		   		read_chars = COMMON;
		   		state = CONTENT_DATA;
		   		compBuf.index = 0;
    			compBuf.buff[0] = 0;
		   		return 0;
		   	}
		   	if (LIMIT(c)){
		   		error();
		   		return 1;
		   	}
			break;
		case HEADER_DATA:
			if(read_chars == NEW_LINE){
				read_chars = COMMON;
				state = HEADER_NAME;
				compBuf.index = 0;
    			compBuf.buff[0] = 0;
    			write_to_comp_buff(c, compBuf);
				return transition(c, transformed_mail);
			}
			if (read_chars == CRLFCRLF) {
				read_chars = COMMON;
		   		compBuf.index = 0;
    			compBuf.buff[0] = 0;
					if (strcicmp(actualContent.type,"message", &compBuf) == 0){
						state = HEADER_NAME;
					} else {
						state = CONTENT_DATA;
					}
		   		return 0;
			}
			break;
		case CTYPE_DATA:
			if(c == '/') {
				if(actualContent.type != NULL) {
					free(actualContent.type);
				}
				actualContent.type = (char*)malloc((strlen(compBuf.buff) + 1) * sizeof(char));
				strcpy(actualContent.type, compBuf.buff);
				state = CSUBTYPE_DATA;
				compBuf.index = 0;
	    		compBuf.buff[0] = 0;
				return 0;
			}
			if (LIMIT(c)){
		   		error();
		   		return 1;
		   	}
			break;
		case CSUBTYPE_DATA:
			if(c == ';' || read_chars == NEW_LINE || read_chars == CRLFCRLF){
				if(actualContent.subtype != NULL) {
					free(actualContent.subtype);
				}
				actualContent.subtype = (char*)malloc((strlen(compBuf.buff) + 1) * sizeof(char));
				strcpy(actualContent.subtype, compBuf.buff);
				if(is_in_blacklist(blacklist, &actualContent, compBuf) == 0) {
					write_str_to_out_buff(" text/plain\r\n\r\n", printBuf, transformed_mail);
					write_str_to_out_buff(substitute_text, printBuf, transformed_mail);
					write_str_to_out_buff("\r\n", printBuf, transformed_mail);
					cType->type = DISCRETE;
					erasing = ERASING;
					state = CONTENT_DATA;
					return 0;
				}
				if(strcicmp(actualContent.type,"multipart", &compBuf) == 0) {
					cType->type = COMPOSITE;
					cType->next = malloc(sizeof(*(cType->next)));
					cType->next->prev = cType;
					cType->next->next = NULL;
					cType->boundary = NULL;
					cType->next->type = NO_CONTENT;
				} else {
					cType->type = DISCRETE;
				}
				write_str_to_out_buff(actualContent.type, printBuf, transformed_mail);
				write_str_to_out_buff("/", printBuf, transformed_mail);
				write_str_to_out_buff(actualContent.subtype, printBuf, transformed_mail);

				if(c == ';') {
					state = ATTR_NAME;
					compBuf.index = 0;
	    			compBuf.buff[0] = 0;
					write_str_to_out_buff(";", printBuf, transformed_mail);
					return 0;
				}
				if(read_chars == NEW_LINE){
					read_chars = COMMON;
					state = HEADER_NAME;
					compBuf.index = 0;
	    			compBuf.buff[0] = 0;
					write_str_to_out_buff("\r\n", printBuf, transformed_mail);
					write_to_out_buff(c, printBuf, transformed_mail);
					write_to_comp_buff(c, compBuf);
					return transition(c, transformed_mail);
				}
				if (read_chars == CRLFCRLF) {
					read_chars = COMMON;
		   		write_str_to_out_buff("\r\n\r\n", printBuf, transformed_mail);
		   		compBuf.index = 0;
    			compBuf.buff[0] = 0;
					if (strcicmp(actualContent.type,"message", &compBuf) == 0){
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
				if(strcicmp(compBuf.buff,"boundary", &compBuf) == 0) {
					state = BOUNDARY;
				} else {
					state = ATTR_DATA;
				}
				return 0;
			}
			if (LIMIT(c)){
		   		error();
		   		return 1;
		   	}
			break;
		case ATTR_DATA:
			if(c == ';') {
				state = ATTR_NAME;
				compBuf.index = 0;
	    		compBuf.buff[0] = 0;
				return 0;
			}
			if(read_chars == NEW_LINE){
				read_chars = COMMON;
				state = HEADER_NAME;
				compBuf.index = 0;
	    		compBuf.buff[0] = 0;
				write_to_comp_buff(c, compBuf);
				return transition(c, transformed_mail);
			}
			if (read_chars == CRLFCRLF) {
				read_chars = COMMON;
	   		compBuf.index = 0;
  			compBuf.buff[0] = 0;
				if (strcicmp(actualContent.type,"message", &compBuf) == 0){
  				state = HEADER_NAME;
  			} else {
  				state = CONTENT_DATA;
  			}
	   		return 0;
			}
			break;
		case BOUNDARY:
			if(c == ';' || read_chars == NEW_LINE || read_chars == CRLFCRLF){
				//printf("%d\n", read_chars);
 			  cType->boundary = malloc((strlen(compBuf.buff) + 1) * sizeof(char));
				strcpy(cType->boundary, compBuf.buff);
				compBuf.index = 0;
    			compBuf.buff[0] = 0;
				if(c == ';') {
					state = ATTR_NAME;
					compBuf.index = 0;
	    			compBuf.buff[0] = 0;
					return 0;
				}
				if(read_chars == NEW_LINE){
					read_chars = COMMON;
					state = HEADER_NAME;
					compBuf.index = 0;
	    			compBuf.buff[0] = 0;
					write_to_comp_buff(c, compBuf);
					return transition(c, transformed_mail);
				}
				if (read_chars == CRLFCRLF) {
					read_chars = COMMON;
		   		compBuf.index = 0;
    			compBuf.buff[0] = 0;
					if (strcicmp(actualContent.type,"message", &compBuf) == 0){
						state = HEADER_NAME;
					} else {
						state = CONTENT_DATA;
					}
		   		return 0;
				}
			}
			break;
		case CONTENT_DATA:
			if(read_chars == CRLF || read_chars == CRLFCRLF) {
				if(cType->type == COMPOSITE && check_boundary(compBuf.buff, cType->boundary) == SEPARATOR) {
					if(erasing == ERASING){
						write_str_to_out_buff("--", printBuf, transformed_mail);
						write_str_to_out_buff(cType->boundary, printBuf, transformed_mail);
					}
					cType = cType->next;
					cType->boundary = NULL;
					state = HEADER_NAME;
					erasing = NOT_ERASING;
				} else if (cType->type != COMPOSITE && cType->prev != NULL) {
					boundary_type checked_bound = check_boundary(compBuf.buff, cType->prev->boundary);
					if(checked_bound == SEPARATOR){
						if(erasing == ERASING){
							write_str_to_out_buff("--", printBuf, transformed_mail);
							write_str_to_out_buff(cType->prev->boundary, printBuf, transformed_mail);
							write_str_to_out_buff("\r\n", printBuf, transformed_mail);
						}
						state = HEADER_NAME;
						erasing = NOT_ERASING;
					} else if(checked_bound == FINISH) {
						if(erasing == ERASING){
							write_str_to_out_buff("--", printBuf, transformed_mail);
							write_str_to_out_buff(cType->prev->boundary, printBuf, transformed_mail);
							write_str_to_out_buff("--\r\n", printBuf, transformed_mail);
						}
						free(cType->prev->boundary);
						cType->prev->boundary = NULL;
						cTypeStack* aux1 = cType;
						cTypeStack* aux2 = cType->prev;
						cType = cType->prev->prev;
						free(aux1);
						aux2->next = NULL;
						if(cType == NULL) {
							free(aux2);
							state = TRANSPARENT;
						}
						erasing = NOT_ERASING;
					}
				}
				compBuf.index = 0;
    			compBuf.buff[0] = 0;
    			if(state == HEADER_NAME){
    				read_chars = COMMON;
    			}
			}
			break;
	}
	return 0;
}
