#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stdio.h>
#include "parser_types.h"

void write_to_out_buff(char c, Buffer * print_buffer, FILE * transformed_mail);
void write_str_to_out_buff(char* str, Buffer * print_buffer, FILE * transformed_mail);
void write_to_comp_buff(char c, Buffer * comparison_buffer);
void clear_buffer(Buffer * buffer_pointer);

#endif
