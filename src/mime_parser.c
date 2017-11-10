#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "parser_utils.h"

#define CHARACTER_SIZE 1

typedef enum {HEADER_NAME=0, HEADER_DATA, CTYPE_DATA, CSUBTYPE_DATA, ATTR_NAME, ATTR_DATA, BOUNDARY, CONTENT_DATA, TRANSPARENT} states;
typedef enum {NO_CONTENT, COMPOSITE, DISCRETE} content_type;
typedef enum {SEPARATOR, FINISH, NOT_BOUNDARY} boundary_type;
typedef enum {NOT_ERASING, ERASING} errase_action;
typedef enum {CR, CRLF, CRLFCR, CRLFCRLF, FOLD, COMMON, NEW_LINE} char_types;

typedef struct cTypeQueue{
	struct cTypeQueue* prev;
	struct cTypeQueue* next;
	content_type type;
	char* boundary;
} cTypeQueue;

typedef struct cTypeNSubType{
	char* type;
	char* subtype;
} cTypeNSubType;

#define BUFSIZE 1024
#define BUFFLEN 1000

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
cTypeQueue* cType;
cTypeNSubType* blackList[100];
cTypeNSubType actualContent;
char* substitute_text;
int erasing = 0;

#define LIMIT(c) ((c) == ':' || (c) == ';' || (c) == 0 || (c) == '=' || (c) == '/' || (c) == ',')
#define LIMIT_BOUND(c) ((c) == '"' || (c) == 0 || (c) == '\r' || (c) == '\n')
#define WHITESPACE(c) ((c) == ' ' || (c) == '	')
#define CRLF_CHAR(c) ((c) == '\n' || (c) == '\r')

void free_queue_node(cTypeQueue* node) {
	if(node->boundary != NULL){
		free(node->boundary);
	}
	free(node);
}

void free_blacklist() {
	for(int k = 0; blackList[k] != NULL; k++) {
		free(blackList[k]->type);
		free(blackList[k]->subtype);
   		free(blackList[k]);
   	}
}

/*
	Case insensitive string compare
*/
int strcicmp(char *a, char const *b)
{
	int ans = 0;
	//printf("%s\n", a);
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

/*
	Crea una lista con todos los Content Types que debe censurar.
*/
void populate_blacklist(char* items) {
	int i = 0;
	while(items[0] != 0) {
		items = copy_to_buffer(items);
		if(items[0] == '/') {
			blackList[i] = malloc(sizeof(cTypeNSubType));
			blackList[i]->type = (char*)malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(blackList[i]->type, buff);
			items++;
		} else {
			blackList[i]->subtype = (char*)malloc((strlen(buff) + 1) * sizeof(char));
			strcpy(blackList[i]->subtype, buff);
			if(items[0] == ',') {
				items++;
			}
			i++;
		}
	}
	blackList[i] = NULL;
}

/*
	Verifica si un contenido dado debe ser censurado o no.
*/
int isInBlacklist(cTypeNSubType* content) {
	for(int k = 0; blackList[k] != NULL; k++) {
   		//printf("\nType: %s, Sub: %s\n", blackList[k]->type, blackList[k]->subtype);
   		if(strcicmp(content->type, blackList[k]->type) == 0){
   			if(blackList[k]->subtype[0] == '*' || strcicmp(content->subtype, blackList[k]->subtype) == 0){
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

void write_to_comp_buff(char c){
	if(compBuf.index + 1 >= BUFFLEN){
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



int transition(char c, FILE * transformed_mail){
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
			if(c == ';' || read_chars == NEW_LINE || read_chars == CRLF){
				if(actualContent.subtype != NULL) {
					free(actualContent.subtype);
				}
				actualContent.subtype = (char*)malloc((strlen(compBuf.buff) + 1) * sizeof(char));
				strcpy(actualContent.subtype, compBuf.buff);
				if(isInBlacklist(&actualContent) == 0) {
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
			   		write_str_to_out_bufff("\r\n", transformed_mail);
			   		compBuf.index = 0;
	    			compBuf.buff[0] = 0;
			   		return 0;
				}
			}
			break;
		case ATTR_NAME:
			if(c == '='){
				//printf("\n%s|%d|\n", compBuf.buff,(int) strlen(compBuf.buff));
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
				cType->boundary = malloc(100 * sizeof(char));
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
			if(read_chars == CRLF) {
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
						cTypeQueue* aux1 = cType->prev;
						cTypeQueue* aux2 = cType;
						cType = cType->prev->prev;
						free_queue_node(aux1);
						free_queue_node(aux2);
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

#define BUFSIZE 1024

char aux[BUFSIZE] = { 0 };

int mime_parser(char * filter_medias, char * filter_message, char * client_number, FILE * retrieved_mail) {
	char c;
	int i = 0;
	state = HEADER_NAME;
	read_chars = COMMON;

	// char * retrieved_mail_file_path  = get_retrieved_mail_file_path(client_number[0]);
  char * transformed_mail_file_path = get_transformed_mail_file_path(client_number[0]);

	// FILE * retrieved_mail   = fopen(retrieved_mail_file_path, "r");
	FILE * transformed_mail = fopen(transformed_mail_file_path, "a");

 	blackList[0] = NULL;
 	// populate_blacklist("text/html,application/*");
	// substitute_text = "Content was deleted";


	if (filter_medias != NULL) populate_blacklist(filter_medias);
	substitute_text = filter_message;

	actualContent.type = NULL;
	actualContent.subtype = NULL;
 	cType = malloc(sizeof(cTypeQueue));
 	cType->prev = cType->next = NULL;
 	cType->type = NO_CONTENT;
	cType->boundary = NULL;
 	printBuf.index = compBuf.index = 0;
 	int number_read;
 	int is_comment = 1;

	// Eat first characters which correspond to pop3 +ok/-err stuff
	// fread(aux, CHARACTER_SIZE, index_from_which_to_read_file, retrieved_mail);

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

 	return 0;
}