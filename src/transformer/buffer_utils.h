#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stdio.h>
#include "parser_types.h"

void write_to_out_buff(char c, buffer_type * print_buffer, FILE * transformed_mail);
void write_str_to_out_buff(char* str, buffer_type * print_buffer, FILE * transformed_mail);
void write_to_comparison_buffer(char c, buffer_type * comparison_buffer);
void clear_buffer(buffer_type * buffer_pointer);

#endif
