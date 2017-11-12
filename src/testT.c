#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define BUFSIZE 1024

void get_env_vars(char **med, char **msg, char **ver, char **ser, char **name, char **client) {
	*client	= getenv("CLIENT_NUM");
	*med   	= getenv("FILTER_MEDIAS");
	*msg   	= getenv("FILTER_MSG");
	*ver   	= getenv("POP3FILTER_VERSION");
	*ser   	= getenv("POP3_SERVER");
	*name  	= getenv("POP3_USERNAME");
}

void print_env_vars(char *med, char *msg, char *ver, char *ser, char *name, char *client) {
	if (client != NULL) {
		printf("CLIENT_NUM: %s\n", client);
	}
	if (med != NULL) {
		printf("FILTER_MEDIAS: %s\n", med);
	}
	if (msg != NULL) {
		printf("FILTER_MSG: %s\n", msg);
	}
	if (ver != NULL) {
		printf("POP3FILTER_VERSION: %s\n", ver);
	}
	if (ser != NULL) {
		printf("POP3_SERVER: %s\n", ser);
	}
	if(name != NULL) {
		printf("POP3_USERNAME: %s\n", name);
	}
	printf("\n");
}

char aux[BUFSIZE] = { 0 };

int main(){
	char *med, *msg, *ver, *ser, *name, *client;
	int number_read, num_size;

	get_env_vars(&med, &msg, &ver, &ser, &name, &client);
	printf("\n\nTEST PROGRAM OK\n\n");
	print_env_vars(med, msg, ver, ser, name, client);

///////////////////////////////////////////////////////

	#define MAIL_CLIENT_INDEX 15
	char file_path_retr[]   = "./retr_mail_nnnn";
    char file_path_resp[] 	= "./resp_mail_nnnn";

	// Replaces "nnnn" for client id
    num_size = strlen(client);
    for( int q = MAIL_CLIENT_INDEX; q > 11; q--){
        if(num_size > 0) {
            file_path_retr[q] = client[num_size-1];
            file_path_resp[q] = client[num_size-1];
            num_size--;
        }
        else {
            file_path_retr[q] = 48;
            file_path_resp[q] = 48;
        }
    }

	FILE *retrieved_mail   = fopen(file_path_retr, "r");
	FILE *transformed_mail = fopen(file_path_resp, "a");

	#define CHARACTER_SIZE 1
	#define READ_COUNT BUFSIZE - 1

  while ((number_read = fread(aux, CHARACTER_SIZE, READ_COUNT, retrieved_mail)) > 0) {
      fwrite(aux, 1, number_read, transformed_mail);

      for(int p = 0 ; p < BUFSIZE ; p++ )
          aux[p] = '\0';
  }

  fclose(retrieved_mail);
  fclose(transformed_mail);

	return 0;
}
