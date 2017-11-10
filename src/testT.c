#include <stdio.h>
#include <stdlib.h>
#define BUFSIZE 128

int main(){
	char *med, *msg, *ver, *ser, *name, *client;
	char aux[BUFSIZE];
	int nread;
	FILE *retrieved_mail;
	FILE *transformed_mail;

	for( int p = 0 ; p < BUFSIZE ; p++ ) {
        aux[p] = '\0';
    }

    //Testing the environment variables only
    //-------------------------------------------------
	printf("\n\nTEST PROGRAM OK\n\n");
	client 	= getenv("CLIENT_NUM");
	med  	= getenv("FILTER_MEDIAS");
	msg 	= getenv("FILTER_MSG");
	ver  	= getenv("POP3FILTER_VERSION");
	ser  	= getenv("POP3_SERVER");
	name 	= getenv("POP3_USERNAME");

	printf("CLIENT_NUM: %s\n", client);
	if(med != NULL)
		printf("FILTER_MEDIAS: %s\n", med);
	printf("FILTER_MSG: %s\n", msg);
	printf("POP3FILTER_VERSION: %s", ver);
	printf("POP3_SERVER: %s\n", ser);
	if(name != NULL)
		printf("POP3_USERNAME: %s\n", name);
	printf("\n");
	//-------------------------------------------------

	char file_path_retr[]   = "./retrieved_mail_n.txt";
    char file_path_transf[] = "./transformed_mail_n.txt";
    file_path_retr[17]      = client[0];
    file_path_transf[19]    = client[0];
    retrieved_mail          = fopen(file_path_retr, "r");
    transformed_mail        = fopen(file_path_transf, "a");

    while ((nread = fread(aux, 1, BUFSIZE-1, retrieved_mail)) > 0){
        fwrite( aux, 1, nread, transformed_mail );
        for( int p = 0 ; p < BUFSIZE ; p++ )
            aux[p] = '\0';
    }
    fclose(retrieved_mail);
    fclose(transformed_mail);

	return 0;
}