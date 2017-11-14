#include <stdlib.h>
#include <string.h>
#include "parser_types.h"
#include "debug.h"

char * state_to_string(state_type state) {
  char * str = (char *)malloc(20 * sizeof(char));

  if (state == HEADER_NAME) {
    strcpy(str, "HEADER_NAME");
  }
  if (state == HEADER_VALUE) {
    strcpy(str, "HEADER_VALUE");
  }
  if (state == CONTENT_TYPE_TYPE) {
    strcpy(str, "CONTENT_TYPE_TYPE");
  }
  if (state == CONTENT_TYPE_SUBTYPE) {
    strcpy(str, "CONTENT_TYPE_SUBTYPE");
  }
  if (state == ATTRIBUTE_NAME) {
    strcpy(str, "ATTRIBUTE_NAME");
  }
  if (state == ATTRIBUTE_VALUE) {
    strcpy(str, "ATTRIBUTE_VALUE");
  }
  if (state == BOUNDARY) {
    strcpy(str, "BOUNDARY");
  }
  if (state == CONTENT_TYPE_BODY) {
    strcpy(str, "CONTENT_TYPE_BODY");
  }
  if (state == POSSIBLE_ERROR) {
    strcpy(str, "POSSIBLE_ERROR");
  }

  return str;
}
