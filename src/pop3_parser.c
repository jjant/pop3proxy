#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "parser_utils.h"
#include "mime_parser.h"

#define CHARACTER_SIZE 1
#define READ_COUNT POP3_TOP_CAPACITY
#define POP3_TOP_CAPACITY 512
#define MAX_LENGTH POP3_TOP_CAPACITY + 1

const char * ok_status = "+OK";
const char * err_status = "-ERR";

// Copies the first line of the file and deletes it.
// This is so we don't have to deal with POP3 stuff in the mime parser (+OK, -ERR).
int main() {
  char buffer[MAX_LENGTH] = "";
  char *filter_medias, *filter_message, *pop3_filter_version, *pop3_server, *pop3_username, *client_number;

  get_env_vars(&filter_medias, &filter_message, &pop3_filter_version, &pop3_server, &pop3_username, &client_number);

  char * retrieved_mail_file_path  = get_retrieved_mail_file_path(client_number);
  char * transformed_mail_file_path = get_transformed_mail_file_path(client_number);

  remove(transformed_mail_file_path);

	FILE * retrieved_mail   = fopen(retrieved_mail_file_path, "r");
	FILE * transformed_mail = fopen(transformed_mail_file_path, "w");

  int number_read = 0;
  int buffer_index = 0;

  while ((number_read = fread(buffer, CHARACTER_SIZE, READ_COUNT, retrieved_mail)) > 0) {
    while (buffer[buffer_index] != '\0') {
      if (buffer[buffer_index] == '\r' && buffer[buffer_index + 1] == '\n') {
          goto end;
      }
      buffer_index++;
    }
  }

end:
  fwrite(buffer, CHARACTER_SIZE, buffer_index + 2, transformed_mail);

  fclose(retrieved_mail);
  fclose(transformed_mail);
  mime_parser(filter_medias, filter_message, client_number, buffer_index + 2);
  return 0;
}
