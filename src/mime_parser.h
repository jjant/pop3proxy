#ifndef MIME_PARSER_H
#define MIME_PARSER_H

#include <stdlib.h>
#include <stdio.h>

int mime_parser(char * filter_medias, char * filter_message, char * client_number, FILE * retrieved_mail);

#endif
