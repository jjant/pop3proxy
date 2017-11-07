#include <stdio.h>
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
	int number_read;

	get_env_vars(&med, &msg, &ver, &ser, &name, &client);
	printf("\n\nTEST PROGRAM OK\n\n");
	print_env_vars(med, msg, ver, ser, name, client);

///////////////////////////////////////////////////////

	#define RETRIEVED_MAIL_CLIENT_INDEX 17
	#define TRANSFORMED_MAIL_CLIENT_INDEX 19
	char retrieved_mail_file_path[]   = "./retrieved_mail_n.txt";
  char transformed_mail_file_path[] = "./transformed_mail_n.txt";

	// Replaces "n" for client id
  retrieved_mail_file_path[RETRIEVED_MAIL_CLIENT_INDEX]      = client[0];
  transformed_mail_file_path[TRANSFORMED_MAIL_CLIENT_INDEX]  = client[0];

	FILE *retrieved_mail   = fopen(retrieved_mail_file_path, "r");
	FILE *transformed_mail = fopen(transformed_mail_file_path, "a");

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
