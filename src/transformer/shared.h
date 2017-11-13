#ifndef SHARED_H
#define SHARED_H

#include "mime_parser.h"

#define CHARACTER_SIZE 1
#define LIMIT(c) ((c) == ':' || (c) == ';' || (c) == 0 || (c) == '=' || (c) == '/' || (c) == ',')
#define LIMIT_BOUND(c) ((c) == '"' || (c) == 0 || (c) == '\r' || (c) == '\n')
#define WHITESPACE(c) ((c) == ' ' || (c) == '	')
#define CRLF_CHAR(c) ((c) == '\n' || (c) == '\r')

#endif
