#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

char * get_retrieved_mail_file_path(char client_number) {
    char * prototype_path = "./retrieved_mail_n.txt";
  	char * retrieved_mail_file_path = malloc(strlen(prototype_path));
    strcpy(retrieved_mail_file_path, prototype_path);
    #define RETRIEVED_MAIL_CLIENT_INDEX 17
    retrieved_mail_file_path[RETRIEVED_MAIL_CLIENT_INDEX] = client_number;

    return retrieved_mail_file_path;
}

char * get_transformed_mail_file_path(char client_number) {
    char * prototype_path = "./transformed_mail_n.txt";
  	char * transformed_mail_file_path = malloc(strlen(prototype_path));
    strcpy(transformed_mail_file_path, prototype_path);
  	#define TRANSFORMED_MAIL_CLIENT_INDEX 19
    transformed_mail_file_path[TRANSFORMED_MAIL_CLIENT_INDEX] = client_number;

    return transformed_mail_file_path;
}
