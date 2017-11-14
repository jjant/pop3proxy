#include <string.h>
#include "buffer_utils.h"
#include "parser_types.h"
#include "shared.h"

void clear_buffer(buffer_type * buffer_pointer) {
	buffer_pointer->index = 0;
	buffer_pointer->buffer[0] = 0;
}

void out_buffer_write(char c, buffer_type * output_buffer, FILE * transformed_mail) {
	if (output_buffer->index + 1 >= BUFFER_SIZE) {
		const char * buffer = output_buffer->buffer;
		fwrite(buffer, CHARACTER_SIZE, strlen(buffer), transformed_mail);
		output_buffer->index = 0;
	}
	output_buffer->buffer[output_buffer->index++] = c;
	output_buffer->buffer[output_buffer->index] = 0;
}

void out_buffer_str_write(char* str, buffer_type * output_buffer, FILE * transformed_mail) {
	for(size_t i=0; i < strlen(str); i++) {
		out_buffer_write(str[i], output_buffer, transformed_mail);
	}
}

void write_to_helper_buffer(char c, buffer_type * helper_buffer) {
	if(helper_buffer->index + 1 >= BUFFER_SIZE) {
		helper_buffer->index = 0;
	}
	helper_buffer->buffer[helper_buffer->index++] = c;
	helper_buffer->buffer[helper_buffer->index] = 0;
}
