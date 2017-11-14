#include "validations.h"
#include "parser_types.h"
#include "mime_state_machine.h"
#include "shared.h"

void error(state_type * state) {
	*state = POSSIBLE_ERROR;
}

char_types get_current_token(char_types character_token, char c) {
	if((character_token == COMMON || character_token == FOLD) && c == '\r'){
		return CR;
	}
	if(character_token == CR && c == '\n'){
		return CRLF;
	}
	if(character_token == CRLF && c == '\r'){
		return CRLFCR;
	}
	if(character_token == CRLFCR && c == '\n'){
		return CRLFCRLF;
	}
	if((character_token == CRLF || character_token == FOLD) && is_white_space(c)){
		return FOLD;
	}
	if(character_token == CRLF && !is_white_space(c)){
		return NEW_LINE;
	}
	return COMMON;
}

void handle_maybe_message(state_type * state, bool is_message) {
	if (is_message) {
		*state = HEADER_NAME;
	} else {
		*state = CONTENT_TYPE_BODY;
	}
}

int state_transition(state_type * state, char c, FILE * transformed_mail) {
	switch(*state) {
		case POSSIBLE_ERROR:
			return 0;
		case HEADER_NAME:
			if(c == ':'){
				bool is_content_type = strcicmp(comparison_buffer.buff, "Content-Type") == 0;
				clear_buffer(&comparison_buffer);
	   		if (is_content_type) {
					state = CONTENT_TYPE_TYPE;
				} else {
					state = HEADER_VALUE;
				}
				return 0;
		   	}
		   	if(character_token == CRLFCR){
		   		character_token = COMMON;
		   		state = CONTENT_TYPE_BODY;
					clear_buffer(&comparison_buffer);
		   		return 0;
		   	}
		   	if (is_delimiter(c)){
		   		error(state);
		   		return 1;
		   	}
			break;
		case HEADER_VALUE:
			if(character_token == NEW_LINE){
				character_token = COMMON;
				state = HEADER_NAME;
				clear_buffer(&comparison_buffer);
  			write_to_comparison_buffer(c, &comparison_buffer);
				return state_transition(c, transformed_mail);
			}
			if (character_token == CRLFCRLF) {
				bool is_message = strcicmp(current_content_type.type, "message") == 0;
				clear_buffer(&comparison_buffer);
				character_token = COMMON;
				handle_maybe_message(&state, is_message);
	   		return 0;
			}
			break;
		case CONTENT_TYPE_TYPE:
			if(c == '/') {
				if(current_content_type.type != NULL) {
					free(current_content_type.type);
				}
				current_content_type.type = allocate_for_string(comparison_buffer.buff);
				strcpy(current_content_type.type, comparison_buffer.buff);
				state = CONTENT_TYPE_SUBTYPE;
				clear_buffer(&comparison_buffer);
				return 0;
			}
			if (is_delimiter(c)){
		   		error(state);
		   		return 1;
		   	}
			break;
		case CONTENT_TYPE_SUBTYPE:
			if(c == ';' || character_token == NEW_LINE || character_token == CRLFCRLF){
				if(current_content_type.subtype != NULL) {
					free(current_content_type.subtype);
				}
				current_content_type.subtype = (char*)malloc((strlen(comparison_buffer.buff) + 1) * sizeof(char));
				strcpy(current_content_type.subtype, comparison_buffer.buff);
				bool is_content_in_filter_list = is_in_filter_list(filter_list, &current_content_type);
				clear_buffer(&comparison_buffer);
				if (is_content_in_filter_list) {
					write_str_to_out_buff(" text/plain\r\n\r\n", &print_buffer, transformed_mail);
					write_str_to_out_buff(filter_replacement_message, &print_buffer, transformed_mail);
					write_str_to_out_buff("\r\n", &print_buffer, transformed_mail);
					content_type_stack->type = DISCRETE;
					erasing = true;
					state = CONTENT_TYPE_BODY;
					return 0;
				}
				bool is_multipart = strcicmp(current_content_type.type,"multipart") == 0;
				clear_buffer(&comparison_buffer);
				if (is_multipart) {
					content_type_stack->type = COMPOSITE;
					content_type_stack->next = malloc(sizeof(*(content_type_stack->next)));
					content_type_stack->next->prev = content_type_stack;
					content_type_stack->next->next = NULL;
					content_type_stack->boundary = NULL;
					content_type_stack->next->type = NO_CONTENT;
				} else {
					content_type_stack->type = DISCRETE;
				}
				write_str_to_out_buff(current_content_type.type, &print_buffer, transformed_mail);
				write_str_to_out_buff("/", &print_buffer, transformed_mail);
				write_str_to_out_buff(current_content_type.subtype, &print_buffer, transformed_mail);

				if(c == ';') {
					state = ATTRIBUTE_NAME;
					clear_buffer(&comparison_buffer);
					write_str_to_out_buff(";", &print_buffer, transformed_mail);
					return 0;
				}
				if(character_token == NEW_LINE){
					character_token = COMMON;
					state = HEADER_NAME;
					clear_buffer(&comparison_buffer);
					write_str_to_out_buff("\r\n", &print_buffer, transformed_mail);
					write_to_out_buff(c, &print_buffer, transformed_mail);
					write_to_comparison_buffer(c, &comparison_buffer);
					return state_transition(c, transformed_mail);
				}
				if (character_token == CRLFCRLF) {
					character_token = COMMON;
		   		write_str_to_out_buff("\r\n\r\n", &print_buffer, transformed_mail);
					bool is_message = strcicmp(current_content_type.type,"message") == 0;
					clear_buffer(&comparison_buffer);
					handle_maybe_message(&state, is_message);
		   		return 0;
				}
			}
			break;
		case ATTRIBUTE_NAME:
			if(c == '='){
				bool is_boundary = strcicmp(comparison_buffer.buff,"boundary") == 0;
				clear_buffer(&comparison_buffer);
				if(is_boundary) {
					state = BOUNDARY;
				} else {
					state = ATTRIBUTE_VALUE;
				}
				return 0;
			}
			if (is_delimiter(c)){
		   		error(state);
		   		return 1;
		   	}
			break;
		case ATTRIBUTE_VALUE:
			if(c == ';') {
				state = ATTRIBUTE_NAME;
				clear_buffer(&comparison_buffer);
				return 0;
			}
			if(character_token == NEW_LINE){
				character_token = COMMON;
				state = HEADER_NAME;
				clear_buffer(&comparison_buffer);
				write_to_comparison_buffer(c, &comparison_buffer);
				return state_transition(c, transformed_mail);
			}
			if (character_token == CRLFCRLF) {
				bool is_message = strcicmp(current_content_type.type,"message") == 0;
				clear_buffer(&comparison_buffer);
				character_token = COMMON;
				handle_maybe_message(&state, is_message);
	   		return 0;
			}
			break;
		case BOUNDARY:
			if(c == ';' || character_token == NEW_LINE || character_token == CRLFCRLF){
 			  content_type_stack->boundary = malloc((strlen(comparison_buffer.buff) + 1) * sizeof(char));
				strcpy(content_type_stack->boundary, comparison_buffer.buff);
				clear_buffer(&comparison_buffer);
				if(c == ';') {
					state = ATTRIBUTE_NAME;
					clear_buffer(&comparison_buffer);
					return 0;
				}
				if(character_token == NEW_LINE){
					character_token = COMMON;
					state = HEADER_NAME;
					clear_buffer(&comparison_buffer);
					write_to_comparison_buffer(c, &comparison_buffer);
					return state_transition(c, transformed_mail);
				}
				if (character_token == CRLFCRLF) {
					bool is_message = strcicmp(current_content_type.type,"message") == 0;
					clear_buffer(&comparison_buffer);
					character_token = COMMON;
					handle_maybe_message(&state, is_message);
		   		return 0;
				}
			}
			break;
		case CONTENT_TYPE_BODY:
			if(character_token == CRLF || character_token == CRLFCRLF) {
				if(content_type_stack->type == COMPOSITE && check_boundary(comparison_buffer.buff, content_type_stack->boundary) == SEPARATOR) {
					if(erasing == true){
						write_str_to_out_buff("--", &print_buffer, transformed_mail);
						write_str_to_out_buff(content_type_stack->boundary, &print_buffer, transformed_mail);
					}
					content_type_stack = content_type_stack->next;
					content_type_stack->boundary = NULL;
					state = HEADER_NAME;
					erasing = false;
				} else if (content_type_stack->type != COMPOSITE && content_type_stack->prev != NULL) {
					boundary_type checked_bound = check_boundary(comparison_buffer.buff, content_type_stack->prev->boundary);
					if(checked_bound == SEPARATOR){
						if(erasing == true){
							write_str_to_out_buff("--", &print_buffer, transformed_mail);
							write_str_to_out_buff(content_type_stack->prev->boundary, &print_buffer, transformed_mail);
							write_str_to_out_buff("\r\n", &print_buffer, transformed_mail);
						}
						state = HEADER_NAME;
						erasing = false;
					} else if(checked_bound == FINISH) {
						if(erasing == true){
							write_str_to_out_buff("--", &print_buffer, transformed_mail);
							write_str_to_out_buff(content_type_stack->prev->boundary, &print_buffer, transformed_mail);
							write_str_to_out_buff("--\r\n", &print_buffer, transformed_mail);
						}
						free(content_type_stack->prev->boundary);
						content_type_stack->prev->boundary = NULL;
						content_type_stack_type* aux1 = content_type_stack;
						content_type_stack_type* aux2 = content_type_stack->prev;
						content_type_stack = content_type_stack->prev->prev;
						free(aux1);
						aux2->next = NULL;
						if(content_type_stack == NULL) {
							free(aux2);
							state = POSSIBLE_ERROR;
						}
						erasing = false;
					}
				}
				clear_buffer(&comparison_buffer);
  			if(state == HEADER_NAME){
  				character_token = COMMON;
  			}
			}
			break;
	}
	return 0;
}
