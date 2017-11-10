//Parte de conexión con el proxy

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 128

int readString(char buffer[BUFSIZE]);

//void analyseString(char buffer[BUFSIZE]);

//This is merely a prototype function, to test connectivity with server
int main(int argc, char *argv[]) {
  	struct sockaddr_in 	 server_address;
  	int 				         exit_command			= 0;
  	int 				         sock;
  	char 				         initial_buffer[BUFSIZE];
	//Check for correct amount of args. For now, values are hard coded.
	/*if (argc != 3) { // Test for correct number of arguments
    	perror("Parameter(s) must be <Server Address> <Server Port>\n");
        exit(EXIT_FAILURE);
	}
  	char *servIP = argv[1];     // First arg: server IP address (dotted quad)
  	char *echoString = argv[2]; // Second arg: server port
	*/
  	char *server_IP = "127.0.0.1";
  	int  server_port 	 = 7777;

  	for(int k = 0; k < BUFSIZE ; k++){
  		initial_buffer[k] = '\0';
  	}

  	// Create a reliable, stream socket
  	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_SCTP);
  	if (sock < 0){
    	perror("socket failed");
    	exit(EXIT_FAILURE);
  	}

  	// Construct the server address structure
  	// Zero out structure
  	memset(&server_address, 0, sizeof(server_address)); 
  	// IPv4 address family
  	server_address.sin_family = AF_INET;          
  	// Convert address
  	if (inet_pton(AF_INET, server_IP, &server_address.sin_addr.s_addr) <= 0) {
    	perror("inet_pton failed");
    	exit(EXIT_FAILURE);
  	}
  	// Server port
  	server_address.sin_port = htons(server_port);    

  	// Establish the connection to the server
  	if (connect(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
    	perror("connect failed");
    	exit(EXIT_FAILURE);
  	}

    //Don't remember what's this for, i think because server sends greeting msg
    if ( recv(sock, initial_buffer, BUFSIZE - 1, 0) <= 0) {
    	perror("recv failed");
    	exit(EXIT_FAILURE);
    }
    printf("%s\n", initial_buffer );

}


//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//Parte de lectura de argumentos, la idea es mandar de a un comando por vez al proxy, esperar su respuesta, y enviar lo siguiente hasta terminar.
//Faltan contemplar comandos igual, los de seteo y obtención de métricas por ejemplo, que se podrían dejar en algún lado especial. Verlo..


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

int main(int argc, char *argv[]) {

    FILE *fp;
    
    /*
     * Inicializamos las variables con sus valores predeterminados.
     */
    char **censurable_types;           // -M
    int  censurable_types_idx = 0;     // cantidad de tipos.

    char *error_file          = "/dev/null";  // -e
    char *pop3_address        = "default";    // -l
    char *management_address  = "default";    // -L
    char *message             = "default";    // -m
    char *cmd                 = "default";    // -t

    int option = 0;
    int help = -1;              // -h
    int management_port = 9090; // -o
    int local_port = 1110;      // -p
    int origin_port = 110;      // -P
    int verbose = -1;           // -v

    censurable_types = malloc(sizeof(char*));

    int message_multiple_lines = 0;
    int error = 0;

    /*
     * Algunas variables auxiliares.
     */
    int   index;
    char* next;

    /*
     * Especificamos las opciones. Aquellas que aparecen seguidas de el
     * caracter ':' indican que requieren un argumento.
     */
    while ((option = getopt(argc, argv,"e:hl:L:m:M:o:p:P:t:v")) != -1) {
        switch (option) {
            case 'e' : 
                error_file = malloc(strlen(optarg));
                strcpy(error_file, optarg);
                fp = freopen(error_file, "a", stderr);
                break;
            case 'h' : 
                help = 1;
                break;
            case 'l' :
                pop3_address = malloc(strlen(optarg));
                strcpy(pop3_address, optarg);
                break;
            case 'L' : 
                management_address = malloc(strlen(optarg));
                strcpy(management_address, optarg);
                break;
            case 'm' :
                if (message_multiple_lines == 0) {
                    message_multiple_lines = 1;
                    message = malloc(strlen(optarg));
                    strcpy(message, optarg);
                }
                else {
                    message = realloc(message, 
                        strlen(message) + strlen("\\n") + strlen(optarg));
                    strcat(message, "\n");
                    strcat(message, optarg);
                }
                break;
            case 'M' :
                index = optind - 1;
                while (index < argc) {
                    next = argv[index];
                    index++;
                    if (next[0] != '-') {
                        censurable_types_idx ++;
                        censurable_types = realloc(censurable_types, 
                            censurable_types_idx * sizeof(char*));
                        censurable_types[censurable_types_idx - 1] = malloc(strlen(next));
                        strcpy(censurable_types[censurable_types_idx - 1], next);
                    }
                    else {
                        optind = index - 1;
                        break;
                    }
                }
                break;
            case 'o' :
                management_port = atoi(optarg);
                break;
            case 'p' :
                local_port = atoi(optarg);
                break;
            case 'P' :
                origin_port = atoi(optarg);
                break;
            case 't' :
                cmd = malloc(strlen(optarg));
                strcpy(cmd, optarg);
                break;
            case 'v' :
                verbose = 1;
                break;
            default: 
                printf("error en los parámetros\n");
                error = 1;
                break;
        }
    }
    if (error == 1)
        exit(EXIT_FAILURE);

    printf("error_file: %s\n", error_file);
    if (help == -1)
        printf("help not selected\n");
    else
        printf("help selected\n");
    printf("pop3_address: %s\n", pop3_address);
    printf("management_address: %s\n", management_address);
    printf("message: %s\n", message);

    // Media types
    printf("media-types:");
    for (int i = 0; i < censurable_types_idx; i++) {
        if (i == 0)
            printf(" %s", censurable_types[i]);
        else
            printf(", %s", censurable_types[i]);
    }
    printf("\n");

    printf("management_port: %d\n", management_port);
    printf("local_port: %d\n", local_port);
    printf("origin_port: %d\n", origin_port);
    printf("cmd: %s\n", cmd);
    if (verbose == -1)
        printf("verbose not selected\n");
    else
        printf("verbose selected\n");
    return 0;
}