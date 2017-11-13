#ifndef PARSER_TYPES_H
#define PARSER_TYPES_H

#define BUFFLEN 10000

typedef struct Buffer{
	char buff[BUFFLEN];
	int index;
} Buffer;

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

#endif
