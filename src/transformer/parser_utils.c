#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "shared.h"
#include "parser_types.h"
#include "parser_utils.h"

static char * get_path(char * prototype_path, char * client_number);

int strcicmp(char * a, char const * b, Buffer * comparison_buffer) {
	int answer = 0;
    for (;; a++, b++) {
        int d = tolower(*a) - tolower(*b);
        if (d != 0 || !*a){
            answer = d;
            break;
        }
    }
    comparison_buffer->index = 0;
    comparison_buffer->buff[0] = 0;
    return answer;
}

void get_env_vars(char **filter_medias, char **filter_message, char **pop3_filter_version, char **pop3_server, char **pop3_username, char **client_number) {
	*client_number			 = getenv("CLIENT_NUM");
	*filter_medias   		 = getenv("FILTER_MEDIAS");
	*filter_message   	 = getenv("FILTER_MSG");
	*pop3_filter_version = getenv("POP3FILTER_VERSION");
	*pop3_server   			 = getenv("POP3_SERVER");
	*pop3_username  		 = getenv("POP3_USERNAME");
}

void print_env_vars(char *filter_medias, char *filter_message, char *pop3_filter_version, char *pop3_server, char *pop3_username, char *client_number) {
	printf("\n[[PRINTING ENV VARIABLES FOR DEBUGGING PURPOSES]]\n");
	if (client_number != NULL) {
		printf("CLIENT_NUM: %s\n", client_number);
	}
	if (filter_medias != NULL) {
		printf("FILTER_MEDIAS: %s\n", filter_medias);
	}
	if (filter_message != NULL) {
		printf("FILTER_MSG: %s\n", filter_message);
	}
	if (pop3_filter_version != NULL) {
		printf("POP3FILTER_VERSION: %s\n", pop3_filter_version);
	}
	if (pop3_server != NULL) {
		printf("POP3_SERVER: %s\n", pop3_server);
	}
	if(pop3_username != NULL) {
		printf("POP3_USERNAME: %s\n", pop3_username);
	}
	printf("[[END PRINT ENV VARS]]\n\n");
}

static char * get_path(char * prototype_path, char * client_number) {
	char * new_path = malloc(strlen(prototype_path) + 1);
	strcpy(new_path, prototype_path);

	#define MAIL_CLIENT_INDEX 15
	#define FIRST_N_INDEX 12
	// Replaces "nnnn" for client id
	int num_size = strlen(client_number);

	for (int q = MAIL_CLIENT_INDEX; q >= FIRST_N_INDEX; q--) {
			if (num_size > 0) {
					new_path[q] = client_number[num_size - 1];
					num_size--;
			} else {
					new_path[q] = '0';
			}
	}

	new_path[strlen(prototype_path)] = '\0';
	return new_path;
}

char * get_retrieved_mail_file_path(char * client_number) {
		return get_path("./retr_mail_nnnn", client_number);
}

char * get_transformed_mail_file_path(char * client_number) {
	return get_path("./resp_mail_nnnn", client_number);
}
