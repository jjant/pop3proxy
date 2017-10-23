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
  	struct sockaddr_in 	server_address;
  	int 				exit_command			= 0;
  	int 				sock;
  	char 				initial_buffer[BUFSIZE];
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
  	sock = socket(AF_INET, SOCK_STREAM, 0);
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

    if ( recv(sock, initial_buffer, BUFSIZE - 1, 0) <= 0) {
    	perror("recv failed");
    	exit(EXIT_FAILURE);
    }
    printf("%s\n", initial_buffer );
    
    //Obviously, it's a basic test..
    printf("EVERY COMMAND THAT STARTS WITH 'C + <space>' IS GOING TO BE ECHOED\n");
    printf("TO QUIT, INTRODUCE SOMETHING STARTING WITH 'Q'\n\n");
    
    //This is going to be changed, for now it only needs to connect, send and receive data with server
	while( !exit_command ) {
		char first_char, second_char;
		char buffer[BUFSIZE];
		for( int i = 0; i < BUFSIZE; i++ ){
			buffer[i] = '\0';
		}

		printf("\nWaiting for command...\n");
		printf(">");
		first_char = getchar();
		switch ( toupper(first_char) ){
			case 'C':	if ( (second_char = getchar()) != ' ' ){
							printf("Comando inválido\n");
							while ( getchar() != '\n' );
							break;
						}
						int resp = readString(buffer);
						if (resp > 0) {
							ssize_t num_bytes = send(sock, buffer, sizeof(buffer), 0);
  							if (num_bytes < 0) {
  								perror("send failed");
  								exit(EXIT_FAILURE);
  							}
  							else if (num_bytes != sizeof(buffer)) {
    							perror("sent unexpected number of bytes");
    							exit(EXIT_FAILURE);
  							}

  							// Count of total bytes received
  							unsigned int totalBytesRcvd = 0; 
  							while (totalBytesRcvd < strlen(buffer)) {
    							char resp_buffer[BUFSIZE]; 
    							// Receive up to the buffer size (minus 1 to leave space for a null terminator)
    							num_bytes = recv(sock, resp_buffer, BUFSIZE - 1, 0);
    							if (num_bytes <= 0) {
      								perror("recv failed");
      								exit(EXIT_FAILURE);
    							}
    							totalBytesRcvd += num_bytes;
    							resp_buffer[num_bytes] = '\0';    
    							fputs(resp_buffer, stdout);
  							}
						}
						else {
							printf("Comando inválido (debe ser de menos de %d caracteres)\n", BUFSIZE-1);
							while ( getchar() != '\n' );
						}
						break;
			case 'Q':	printf("Quiting\n");
						close(sock);
						exit_command = 1;
						break;
			default	:	printf("Comando inválido\n");
						while ( getchar() != '\n' );
						break;
		}

	}
	return 0;
}

int readString(char buffer[BUFSIZE]) {
	char c;
	int  i;

	for( i = 0; i < BUFSIZE-1 && (c = getchar()) != '\n'; i++ ){
		buffer[i] = c;	
	}
	if( i != BUFSIZE-1 ){
		buffer[i] = '\n';
		return 1;
	}
	else {
		return 0;
	}
}