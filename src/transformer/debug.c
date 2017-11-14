#include <stdlib.h>
#include <string.h>
#include "parser_types.h"
#include "debug.h"

char * state_to_string(state_type state) {
  char * ret_str;
  char * str;

  if (state == HEADER_NAME) {
    str = "HEADER_NAME";
  }
  if (state == HEADER_VALUE) {
    str = "HEADER_VALUE";
  }
  if (state == CONTENT_TYPE_TYPE) {
    str = "CONTENT_TYPE_TYPE";
  }
  if (state == CONTENT_TYPE_SUBTYPE) {
    str = "CONTENT_TYPE_SUBTYPE";
  }
  if (state == ATTRIBUTE_NAME) {
    str = "ATTRIBUTE_NAME";
  }
  if (state == ATTRIBUTE_VALUE) {
    str = "ATTRIBUTE_VALUE";
  }
  if (state == BOUNDARY) {
    str = "BOUNDARY";
  }
  if (state == CONTENT_TYPE_BODY) {
    str = "CONTENT_TYPE_BODY";
  }
  if (state == POSSIBLE_ERROR) {
    str = "POSSIBLE_ERROR";
  }

  ret_str = malloc((strlen(str) + 2) * sizeof(char));
  strcpy(ret_str, str);
  return ret_str;
}
