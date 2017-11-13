#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stdio.h>
#include "parser_types.h"

void write_to_out_buff(char c, Buffer printBuf, FILE * transformed_mail);
void write_str_to_out_buff(char* str, Buffer printBuf, FILE * transformed_mail);
void write_to_comp_buff(char c, Buffer comparison_buffer);
/*
	El buffer "buff" es donde se copian los fragmentos de strings para su posterior analisis.
	Los strings se fragmentan segun los delimitadores ':', ';', '=' y '/'.
*/
char * copy_to_buffer(char * str, char buffer[]);
#endif
