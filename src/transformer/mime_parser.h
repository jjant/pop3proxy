#ifndef MIME_PARSER_H
#define MIME_PARSER_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "parser_types.h"

// TODO: documentar
int mime_parser(char * filter_medias, char * filter_message, char * client_number, size_t index_from_which_to_read_file);
/* Allows external files to modify the state variable */
void modify_state(state_type new_state);
/* Allows external files to modify the character_token variable */
void modify_character_token(character_token_type new_character_token);
character_token_type get_character_token(void);
int state_transition(char current_character, FILE * transformed_mail);
char * get_filter_replacement_message(void);
void modify_erasing(bool new_erasing);

/* Handles state machine case for ATTRIBUTE_VALUE state */
int handle_attribute_value(char current_character, FILE * transformed_mail);
/* Handles state machine case for BOUNDARY state */
int handle_boundary(char current_character, FILE * transformed_mail);
/* Handles state machine case for CONTENT_TYPE_BODY state */
int handle_content_type_body(char current_character, FILE * transformed_mail);
/* Handles state machine case for CONTENT_TYPE_SUBTYPE state */
int handle_content_type_subtype(char current_character, FILE * transformed_mail);
/* Handles state machine case for CONTENT_TYPE_TYPE state */
int handle_content_type_type(char current_character);

#endif
