#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "parser_utils.h"
#include "mime_parser.h"

#define CHARACTER_SIZE 1

#define BUFSIZE 1024
#define BUFFLEN 10000

static int transition(char c, FILE * transformed_mail);

typedef struct Buffer{
	char buff[BUFFLEN];
	int index;
} Buffer;

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
int erasing = 0;

#define LIMIT(c) ((c) == ':' || (c) == ';' || (c) == 0 || (c) == '=' || (c) == '/' || (c) == ',')
#define LIMIT_BOUND(c) ((c) == '"' || (c) == 0 || (c) == '\r' || (c) == '\n')
#define WHITESPACE(c) ((c) == ' ' || (c) == '	')
#define CRLF_CHAR(c) ((c) == '\n' || (c) == '\r')

void free_blacklist() {
	for(int k = 0; blacklist[k] != NULL; k++) {
		free(blacklist[k]->type);
		free(blacklist[k]->subtype);
   		free(blacklist[k]);
   	}
}

/*
	Case insensitive string compare
*/
int strcicmp(char *a, char const *b) {
	int ans = 0;
    for (;; a++, b++) {
        int d = tolower(*a) - tolower(*b);
        if (d != 0 || !*a){
            ans = d;
            break;
        }
    }
    compBuf.index = 0;
    compBuf.buff[0] = 0;
    return ans;
}

/*
	El buffer "buff" es donde se copian los fragmentos de strings para su posterior analisis.
	Los strings se fragmentan segun los delimitadores ':', ';', '=' y '/'.
*/
char* copy_to_buffer(char* str) {
	int i = 0;
	while(i < BUFFLEN) {
		if(LIMIT(str[0])) {
			buff[i] = 0;
			return str;
		}
		buff[i++] = str[0];
		str++;
	}
	return str;
}

/*
	Verifica si un string es un boundary.
	De serlo, identifica si es un boundary separador o uno final.
*/
boundary_type check_boundary(char* str, char* bound) {
	if(str[0] != 0 && str[1] != 0 && str[0] != '-' && str[1] != '-') {
		return NOT_BOUNDARY;
	}
	str = str + 2;
	int i=0;
	for(; str[i] != 0 && bound[i] != 0; i++) {
		if(str[i] != bound[i]) {
			return NOT_BOUNDARY;
		}
	}
	if(bound[i] == 0 && str[i] != 0 && str[i+1] != 0 && str[i] == '-' && str[i+1] == '-') {
		return FINISH;
	}
	if (bound[i] == 0) {
		return SEPARATOR;
	}
	return NOT_BOUNDARY;
}

static char * allocate_for_string(char * str) {
	return (char*)malloc((strlen(str) + 1) * sizeof(char));
}

static cTypeNSubType * create_blacklist_element(char * type, bool isSubtype) {
	cTypeNSubType * const blacklist_element = malloc(sizeof(cTypeNSubType));

	if (isSubtype) {
		blacklist_element->subtype = allocate_for_string(type);
		strcpy(blacklist_element->subtype, type);
	} else {
		blacklist_element->type = allocate_for_string(type);
		strcpy(blacklist_element->type, type);
	}

	return blacklist_element;
}


/*
	Crea una lista con todos los Content Types que debe censurar.
*/
void populate_blacklist(char* items) {
	int i = 0;

	while(items[0] != '\0') {
		items = copy_to_buffer(items);

		if(items[0] == '/') {
			blacklist[i] = create_blacklist_element(buff, false);
			items++;
		} else {
			blacklist[i]->subtype = (char*)malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(blacklist[i]->subtype, buff);
			if(items[0] == ',') {
				items++;
			}
			i++;
		}
	}

	blacklist[i] = NULL;
}

/*
	Verifica si un contenido dado debe ser censurado o no.
*/
int is_in_blacklist(cTypeNSubType* content) {
	for(int k = 0; blacklist[k] != NULL; k++) {
   		if(strcicmp(content->type, blacklist[k]->type) == 0){
   			if(blacklist[k]->subtype[0] == '*' || strcicmp(content->subtype, blacklist[k]->subtype) == 0){
   				return 0;
   			}
   		}
   	}
   	return 1;
}

void write_to_out_buff(char c, FILE * transformed_mail) {
	if(printBuf.index + 1 >= BUFFLEN){
		const char * buffer = printBuf.buff;
		fwrite(buffer, CHARACTER_SIZE, strlen(buffer), transformed_mail);
		printBuf.index = 0;
	}
	printBuf.buff[printBuf.index++] = c;
	printBuf.buff[printBuf.index] = 0;
}

void write_str_to_out_bufff(char* str, FILE * transformed_mail) {
	for(size_t i=0; i < strlen(str); i++) {
		write_to_out_buff(str[i], transformed_mail);
	}
}

void write_to_comp_buff(char c) {
	if(compBuf.index + 1 >= BUFFLEN) {
		compBuf.index = 0;
	}
	compBuf.buff[compBuf.index++] = c;
	compBuf.buff[compBuf.index] = 0;
}

char_types get_current_state_char(char c){
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

void error() {
	state = TRANSPARENT;
}

#define BUFSIZE 1024

char aux[BUFSIZE] = { 0 };

void handle_error(int number) {
	const char * error_text = "-ERR Connection error\r\n";
	const char * filePath = get_transformed_mail_file_path(number);
	// remove(filePath);
	printf("\n\n\nhola%s\n\n\n\n", filePath);
  FILE * transformed_mail = fopen(filePath, "w");
	fwrite(error_text, CHARACTER_SIZE, strlen(error_text), transformed_mail);
	fclose(transformed_mail);
}

int mime_parser(char * filter_medias, char * filter_message, char * client_number, size_t index_from_which_to_read_file) {
	char c;
	int i = 0;
	state = HEADER_NAME;
	read_chars = COMMON;

	char * retrieved_mail_file_path  = get_retrieved_mail_file_path(client_number[0]);
  char * transformed_mail_file_path = get_transformed_mail_file_path(client_number[0]);

	FILE * retrieved_mail   = fopen(retrieved_mail_file_path, "r");
	FILE * transformed_mail = fopen(transformed_mail_file_path, "a");

 	blacklist[0] = NULL;
 	// populate_blacklist("text/html,application/*");
	// substitute_text = "Content was deleted";


	if (filter_medias != NULL) populate_blacklist(filter_medias);
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
	   		write_to_out_buff(c, transformed_mail);
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
	   			write_to_comp_buff(c);
	   		}

	   	}

			transition(c, transformed_mail);
		}
 	}

	const char * buffer = printBuf.buff;
	fwrite(buffer, CHARACTER_SIZE, strlen(buffer), transformed_mail);
	write(1, printBuf.buff, strlen(printBuf.buff));

	printBuf.index = 0;
	free_blacklist();
	free(actualContent.type);
	free(actualContent.subtype);


  fclose(retrieved_mail);
  fclose(transformed_mail);

	printf("\nstate: %d\n\n", state);
	if (state == TRANSPARENT) {
 		handle_error(client_number[0]);
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
		   		if(strcicmp(compBuf.buff, "Content-Type") == 0) {
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
    			write_to_comp_buff(c);
				return transition(c, transformed_mail);
			}
			if (read_chars == CRLFCRLF) {
				read_chars = COMMON;
		   		state = CONTENT_DATA;
		   		compBuf.index = 0;
    			compBuf.buff[0] = 0;
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
				if(is_in_blacklist(&actualContent) == 0) {
					write_str_to_out_bufff(" text/plain\r\n\r\n", transformed_mail);
					write_str_to_out_bufff(substitute_text, transformed_mail);
					write_str_to_out_bufff("\r\n", transformed_mail);
					cType->type = DISCRETE;
					erasing = ERASING;
					state = CONTENT_DATA;
					return 0;
				}
				if(strcicmp(actualContent.type,"multipart") == 0) {
					cType->type = COMPOSITE;
					cType->next = malloc(sizeof(*(cType->next)));
					cType->next->prev = cType;
					cType->next->next = NULL;
					cType->boundary = NULL;
					cType->next->type = NO_CONTENT;
				} else {
					cType->type = DISCRETE;
				}
				write_str_to_out_bufff(actualContent.type, transformed_mail);
				write_str_to_out_bufff("/", transformed_mail);
				write_str_to_out_bufff(actualContent.subtype, transformed_mail);

				if(c == ';') {
					state = ATTR_NAME;
					compBuf.index = 0;
	    			compBuf.buff[0] = 0;
					write_str_to_out_bufff(";", transformed_mail);
					return 0;
				}
				if(read_chars == NEW_LINE){
					read_chars = COMMON;
					state = HEADER_NAME;
					compBuf.index = 0;
	    			compBuf.buff[0] = 0;
					write_str_to_out_bufff("\r\n", transformed_mail);
					write_to_comp_buff(c);
					return transition(c, transformed_mail);
				}
				if (read_chars == CRLFCRLF) {
					read_chars = COMMON;
			   		state = CONTENT_DATA;
			   		write_str_to_out_bufff("\r\n\r\n", transformed_mail);
			   		compBuf.index = 0;
	    			compBuf.buff[0] = 0;
			   		return 0;
				}
			}
			break;
		case ATTR_NAME:
			if(c == '='){
				if(strcicmp(compBuf.buff,"boundary") == 0) {
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
				write_to_comp_buff(c);
				return transition(c, transformed_mail);
			}
			if (read_chars == CRLFCRLF) {
				read_chars = COMMON;
		   		state = CONTENT_DATA;
		   		compBuf.index = 0;
    			compBuf.buff[0] = 0;
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
					write_to_comp_buff(c);
					return transition(c, transformed_mail);
				}
				if (read_chars == CRLFCRLF) {
					read_chars = COMMON;
			   		state = CONTENT_DATA;
			   		compBuf.index = 0;
	    			compBuf.buff[0] = 0;
			   		return 0;
				}
			}
			break;
		case CONTENT_DATA:
			if(read_chars == CRLF || read_chars == CRLFCRLF) {
				if(cType->type == COMPOSITE && check_boundary(compBuf.buff, cType->boundary) == SEPARATOR) {
					if(erasing == ERASING){
						write_str_to_out_bufff("--", transformed_mail);
						write_str_to_out_bufff(cType->boundary, transformed_mail);
					}
					cType = cType->next;
					cType->boundary = NULL;
					state = HEADER_NAME;
					erasing = NOT_ERASING;
				} else if (cType->type != COMPOSITE && cType->prev != NULL) {
					boundary_type checked_bound = check_boundary(compBuf.buff, cType->prev->boundary);
					if(checked_bound == SEPARATOR){
						if(erasing == ERASING){
							write_str_to_out_bufff("--", transformed_mail);
							write_str_to_out_bufff(cType->prev->boundary, transformed_mail);
							write_str_to_out_bufff("\r\n", transformed_mail);
						}
						state = HEADER_NAME;
						erasing = NOT_ERASING;
					} else if(checked_bound == FINISH) {
						if(erasing == ERASING){
							write_str_to_out_bufff("--", transformed_mail);
							write_str_to_out_bufff(cType->prev->boundary, transformed_mail);
							write_str_to_out_bufff("--\r\n", transformed_mail);
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
