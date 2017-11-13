#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include "parser_types.h"

void get_env_vars(char **, char **, char **, char **, char **, char **);
void print_env_vars(char *, char *, char *, char *, char *, char *);
char * get_retrieved_mail_file_path(char *);
char * get_transformed_mail_file_path(char *);
/* Case insensitive string compare */
int strcicmp(char * a, char const * b, Buffer * comparison_buffer);

#endif
