#include <string.h>
#include "buffer_utils.h"
#include "parser_types.h"
#include "shared.h"

void clear_buffer(Buffer * buffer_pointer) {
	buffer_pointer->index = 0;
	buffer_pointer->buff[0] = 0;
}

void write_to_out_buff(char c, Buffer * print_buffer, FILE * transformed_mail) {
	if (print_buffer->index + 1 >= BUFFLEN) {
		const char * buffer = print_buffer->buff;
		fwrite(buffer, CHARACTER_SIZE, strlen(buffer), transformed_mail);
		print_buffer->index = 0;
	}
	print_buffer->buff[print_buffer->index++] = c;
	print_buffer->buff[print_buffer->index] = 0;
}

void write_str_to_out_buff(char* str, Buffer * print_buffer, FILE * transformed_mail) {
	for(size_t i=0; i < strlen(str); i++) {
		write_to_out_buff(str[i], print_buffer, transformed_mail);
	}
}

void write_to_comp_buff(char c, Buffer * comparison_buffer) {
	if(comparison_buffer->index + 1 >= BUFFLEN) {
		comparison_buffer->index = 0;
	}
	comparison_buffer->buff[comparison_buffer->index++] = c;
	comparison_buffer->buff[comparison_buffer->index] = 0;
}
