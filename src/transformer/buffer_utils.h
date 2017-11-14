#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stdio.h>
#include "parser_types.h"

/* Writes a character to a buffer. If the buffer is full, dumps the buffer to the specified file */
void out_buffer_write(char c, buffer_type * output_buffer, FILE * transformed_mail);
/* Writes a string to a buffer. If the buffer is full, dumps the buffer to the specified file. */
void out_buffer_str_write(char* str, buffer_type * output_buffer, FILE * transformed_mail);
/* Writes a character to a helper buffer. Deletes content if full */
void write_to_helper_buffer(char c, buffer_type * helper_buffer);
/* Clears content of a buffer */
void clear_buffer(buffer_type * buffer_pointer);

#endif
