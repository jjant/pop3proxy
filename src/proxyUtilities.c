#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h> 
#include <arpa/inet.h>   
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>   //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/select.h>
#include <stdbool.h>
#include "proxyUtilities.h"

void initialiseArrays( struct DescriptorsArrays* dA, char buffer[MAXCLIENTS][BUFSIZE]){
    for (int i = 0; i < MAXCLIENTS; i++){
        dA->client_sockets_read[i] = 0;
        dA->client_sockets_write[i] = 0;
        dA->server_sockets_read[i] = 0;
        dA->server_sockets_write[i] = 0;

        for( int j = 0 ; j < BUFSIZE ; j++ ){
            buffer[i][j] = '\0';
        }
        
    }
}

void setSocketsType( struct sockaddr_in* addressClient, struct sockaddr_in* addressServer){
    //type of socket created for client
    addressClient->sin_family = AF_INET;
    addressClient->sin_addr.s_addr = INADDR_ANY;
    addressClient->sin_port = htons( PORT );
    //type of socket created for server. There is only one to test with localhost 110 dovecot
    memset(addressServer, 0, sizeof((*addressServer))); // Zero out structure
    addressServer->sin_family = AF_INET;          // IPv4 address family
    // Convert address
    int rtnVal = inet_pton(AF_INET, "127.0.0.1", &addressServer->sin_addr.s_addr);
    if (rtnVal <= 0){
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }
    addressServer->sin_port = htons(PORTSERVER);    // Server port
}

int addDescriptors( struct DescriptorsArrays* dA, fd_set* readfds, fd_set* writefds){
    int sdc, sds, max_sd;
    max_sd = 0;
    for ( int i = 0 ; i < MAXCLIENTS ; i++){
        //if valid socket descriptor then add to read list
        sdc = dA->client_sockets_read[i];
        sds = dA->server_sockets_read[i];
        if(sdc > 0){
            FD_SET( sdc , readfds);
            if(sdc > max_sd)
                max_sd = sdc;
        }     
        if(sds > 0){
            FD_SET( sds , readfds);
            if(sds > max_sd)
                max_sd = sds;
        }
        //if valid socket descriptor then add to write list
        sdc = dA->client_sockets_write[i];             
        sds = dA->server_sockets_write[i]; 
        if(sdc > 0){
            FD_SET( sdc , writefds);             
            if(sdc > max_sd)
                max_sd = sdc;
        }
        if(sds > 0){
            FD_SET( sds , writefds);             
            if(sds > max_sd)
                max_sd = sds;
        }    
    }
    return max_sd;
}

void handleNewConnection( int master_socket , struct sockaddr_in* addressClient, struct sockaddr_in* addressServer, int* addrClientlen , struct DescriptorsArrays* dA){
	int new_client_socket , new_server_socket;
	char *message = "Connection through proxy... \r\n";

	if ((new_client_socket = accept(master_socket, (struct sockaddr *)addressClient, (socklen_t*)addrClientlen))<0){
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    //create a server socket
    if( (new_server_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0){
        perror("server socket failed");
        exit(EXIT_FAILURE);
    }

    if ((connect(new_server_socket, (struct sockaddr *) addressServer, (socklen_t) sizeof((*addressServer)) ) ) <0 ){
        perror("connect failed");
    }
             
    //send new connection greeting message
    if( write(new_client_socket, message, strlen(message)) != strlen(message) ){
        perror("write greeting message failed");
    }
                          
    //add new socket to array of sockets
    for ( int i = 0; i < MAXCLIENTS; i++){
        //if position is empty
        if( dA->client_sockets_read[i] == 0 ){
            dA->client_sockets_read[i] = new_client_socket;
            dA->server_sockets_read[i] = new_server_socket;
            break;
        }      
    }
          
    printf("New connection: Client socket fd: %d , ip: %s , port: %d\n" , new_client_socket , inet_ntoa(addressClient->sin_addr) , ntohs(addressClient->sin_port));
    printf("                Server socket fd: %d , ip: %s , port: %d\n\n" , new_server_socket , inet_ntoa(addressServer->sin_addr) , ntohs(addressServer->sin_port));
}