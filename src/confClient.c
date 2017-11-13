#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>

#define BUFSIZE 256

//File where metrics are stored
FILE *fp;
int  sock;

void createMessageToSend(char*, char*);
void sendAndReceive(char*, int);

int main(int argc, char *argv[]) {
    struct sockaddr_in 	 proxy_address;
    char                 initial_buffer[BUFSIZE];
    char                 *message                   = NULL;
    //Aux variables
    int                  option                     = 0;
    int                  message_multiple_lines     = 0;
    int                  error                      = 0;
    char                 *proxy_IP;
    int                  proxy_port;

	//Check for correct amount of args.
	if (argc < 3) { // Test for correct number of arguments
    	printf("Parameters must be <Server Address> <Server Port> and <options...>\n");
        exit(EXIT_FAILURE);
	}
    proxy_IP   = argv[1];          // First arg: server IP address (dotted quad)
    proxy_port  = atoi(argv[2]);    // Second arg: server port

    for( int j = 0; j < BUFSIZE; j++)
        initial_buffer[j] = '\0';

    // Create socket
  	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_SCTP);
  	if (sock < 0){
    	perror("Socket failed");
    	exit(EXIT_FAILURE);
  	}

  	// Construct the proxy address structure
  	// Zero out structure
  	memset(&proxy_address, 0, sizeof(proxy_address)); 
  	// IPv4 address family
  	proxy_address.sin_family = AF_INET;          
  	// Convert address
  	if (inet_pton(AF_INET, proxy_IP, &proxy_address.sin_addr.s_addr) <= 0) {
    	perror("inet_pton failed");
    	exit(EXIT_FAILURE);
  	}
  	// Proxy port
  	proxy_address.sin_port = htons(proxy_port);    

  	// Establish the connection to the proxy
  	if (connect(sock, (struct sockaddr *) &proxy_address, sizeof(proxy_address)) < 0) {
    	perror("connect failed");
    	exit(EXIT_FAILURE);
  	}

    if ( recv(sock, initial_buffer, BUFSIZE, 0) <= 0) {
    	perror("recv failed");
    	exit(EXIT_FAILURE);
    }
    printf("%s\n", initial_buffer );    

    fp = fopen("metrics.txt", "w");

    while ((option = getopt(argc, argv,"s:e:hl:L:m:M:o:p:P:t:v123456789")) != -1) {
        switch (option) {
            case 's' :
                createMessageToSend("OS ", optarg);
                break;
            case 'e' :
                createMessageToSend("EF ", optarg); 
                break;
            case 'h' : 
                printf("HELP:\n");
                printf("configClient proxy_address proxy_port [options]\n");
                printf("OPTIONS:\n");
                printf("        -h\n");
                printf("        -v\n");
                printf("        -s origin-server\n");
                printf("        -e archivo-de-error\n");
                printf("        -l dirección-pop3\n");
                printf("        -L dirección-de-management\n");
                printf("        -m mensaje-de-reemplazo\n");
                printf("        -M media-types-censurables\n");
                printf("        -o puerto-de-management\n");
                printf("        -p puerto-local\n");
                printf("        -P puerto-origen\n");
                printf("        -t cmd\n");
                printf("        -1 get-concurrent-connections\n");
                printf("        -2 get-historical-accesses\n");
                printf("        -3 get-transfered-bytes\n");
                printf("        -4 turn-on-concurrent-connections\n");
                printf("        -5 turn-on-historical-accesses\n");
                printf("        -6 turn-on-transfered-bytes\n");
                printf("        -7 turn-off-concurrent-connections\n");
                printf("        -8 turn-off-historical-accesses\n");
                printf("        -9 turn-off-transfered-bytes\n");
                printf("\n");
                exit(0);
                break;
            case 'l' :
                createMessageToSend("PA ", optarg);
                break;
            case 'L' : 
                createMessageToSend("MA ", optarg);
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
                createMessageToSend("CT ", optarg);
                break;
            case 'o' :
                createMessageToSend("MP ", optarg);
                break;
            case 'p' :
                createMessageToSend("PP ", optarg);
                break;
            case 'P' :
                createMessageToSend("OP ", optarg);
                break;
            case 't' :
                createMessageToSend("CD ", optarg);
                break;
            case 'v' :
                sendAndReceive("VN", 1);
                break;
            case '1' :
                sendAndReceive("GCC", 1);
                break;
            case '2' :
                sendAndReceive("GHA", 1);
                break;
            case '3' :
                sendAndReceive("GTB", 1);
                break;
            case '4' :
                sendAndReceive("SCC", 0);
                break;
            case '5' :
                sendAndReceive("SHA", 0);
                break;
            case '6' :
                sendAndReceive("STB", 0);
                break;
            case '7' :
                sendAndReceive("RCC", 0);
                break;
            case '8' :
                sendAndReceive("RHA", 0);
                break;
            case '9' :
                sendAndReceive("RTB", 0);
                break;
            default: 
                error = 1;
                break;
        }
    }
    if (message != NULL){
        createMessageToSend("CT ", message); 
    }

    fclose(fp);
	return 0;
}

void createMessageToSend(char *pref, char *msg) {
    char buf[strlen(msg)+3];
    strcpy(buf, pref);
    strcpy(buf+3, msg);
    sendAndReceive(buf, 0);
}

void sendAndReceive(char *msg, int saveMetric) {
    char resp_buffer[BUFSIZE]; 
    for (int i = 0; i < BUFSIZE; i++)
        resp_buffer[i] = '\0';

    if (send(sock, msg, strlen(msg), 0) < 0)
        perror("send failed");
    if (recv(sock, resp_buffer, BUFSIZE, 0) <= 0) 
        perror("recv failed");
    fputs(resp_buffer, stdout);
    if (saveMetric == 1)
        fwrite(resp_buffer, 1, strlen(resp_buffer), fp);
}