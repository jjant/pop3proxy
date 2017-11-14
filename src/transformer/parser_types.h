#ifndef PARSER_TYPES_H
#define PARSER_TYPES_H

#define BUFFER_SIZE 8192

typedef struct buffer_type {
	int index;
	char buffer[BUFFER_SIZE];
} buffer_type;

typedef enum {
  HEADER_NAME = 0,
  HEADER_VALUE,
  CONTENT_TYPE_TYPE,
  CONTENT_TYPE_SUBTYPE,
  ATTRIBUTE_NAME,
  ATTRIBUTE_VALUE,
  BOUNDARY,
  CONTENT_TYPE_BODY,
  POSSIBLE_ERROR
} state_type;

typedef enum {
  NONE,
  COMPOSITE,
  SIMPLE
} content_type;

typedef enum {
  SEPARATOR,
	NOT_BOUNDARY,
  FINISH,
} boundary_type;

typedef enum {
  CR,
  CRLF,
  CRLFCR,
  CRLFCRLF,
  FOLDING,
  REGULAR,
  NEW_LINE
} character_token_type;

typedef struct content_type_stack_type{
	content_type type;
	char* boundary;
	struct content_type_stack_type * prev;
	struct content_type_stack_type * next;
} content_type_stack_type;

typedef struct content_type_and_subtype{
	char* type;
	char* subtype;
} content_type_and_subtype;

typedef content_type_and_subtype * filter_list_type[200];

#endif
