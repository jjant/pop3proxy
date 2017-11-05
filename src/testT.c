#include <stdio.h>
#include <stdlib.h>

int main(){
	char *med, *msg, *ver, *ser, *name;
	printf("\n\nTEST PROGRAM OK\n\n");
	med  = getenv("FILTER_MEDIAS");
	msg  = getenv("FILTER_MSG");
	ver  = getenv("POP3FILTER_VERSION");
	ser  = getenv("POP3_SERVER");
	name = getenv("POP3_USERNAME");
	
	printf("FILTER_MEDIAS: %s\n", med);
	printf("FILTER_MSG: %s\n", msg);
	printf("POP3FILTER_VERSION: %s", ver);
	printf("POP3_SERVER: %s\n", ser);
	if(name != NULL)
		printf("POP3_USERNAME: %s\n", name);
	printf("\n");
	return 0;
}