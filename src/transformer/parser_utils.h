#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include "parser_types.h"

/* Reads all necessary environment variables. */
void get_env_vars(char **, char **, char **, char **, char **, char **);
/* Prints all necessary environment variables, useful for debugging. */
void print_env_vars(char *, char *, char *, char *, char *, char *);
/* Returns the path for where the retrieved_mail is located, given the client_number */
char * get_retrieved_mail_file_path(char *client_number);
/* Returns the path for where the transformed_mail should go, given the client_number */
char * get_transformed_mail_file_path(char * client_number);
/* Case insensitive string compare: https://stackoverflow.com/questions/5820810/case-insensitive-string-comp-in-c
** We couldn't find a platform-independent built-in function for this.
*/
int strcicmp(char const * a, char const * b);
/* Allocates memory for a string */
char * allocate_for_string(char * str);

#endif
