#ifndef MIME_PARSER_H
#define MIME_PARSER_H

#include <stdlib.h>
#include <stdio.h>

typedef enum {
  HEADER_NAME = 0,
  HEADER_DATA,
  CTYPE_DATA,
  CSUBTYPE_DATA,
  ATTR_NAME,
  ATTR_DATA,
  BOUNDARY,
  CONTENT_DATA,
  TRANSPARENT
} states;

typedef enum {
  NO_CONTENT,
  COMPOSITE,
  DISCRETE
} content_type;

typedef enum {
  SEPARATOR,
  FINISH,
  NOT_BOUNDARY
} boundary_type;

typedef enum {
  NOT_ERASING,
  ERASING
} errase_action;

typedef enum {
  CR,
  CRLF,
  CRLFCR,
  CRLFCRLF,
  FOLD,
  COMMON,
  NEW_LINE
} char_types;

typedef struct cTypeStack{
	struct cTypeStack* prev;
	struct cTypeStack* next;
	content_type type;
	char* boundary;
} cTypeStack;

typedef struct cTypeNSubType{
	char* type;
	char* subtype;
} cTypeNSubType;

// TODO: Documentar
int is_in_blacklist(cTypeNSubType* content);

// TODO: documentar
int mime_parser(char * filter_medias, char * filter_message, char * client_number, size_t index_from_which_to_read_file);

#endif
