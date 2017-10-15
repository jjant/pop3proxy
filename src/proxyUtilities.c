#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h> 
#include <arpa/inet.h>   
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>   //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/select.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/signal.h>
#include "proxyUtilities.h"
#include "handleClient.h"

pthread_t threadForConnection;

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

void setSocketType( struct sockaddr_in* addressClient ){
    //type of socket created for client
    addressClient->sin_family = AF_INET;
    addressClient->sin_addr.s_addr = INADDR_ANY;
    addressClient->sin_port = htons( PORT );
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

void handleNewConnection( int master_socket , struct sockaddr_in* addressClient, int* addrClientlen , struct DescriptorsArrays* dA , fd_set* readfds, fd_set* writefds, pthread_mutex_t* mtx){
    struct ThreadArgs* threadArgs;
    pthread_t tId;

    tId = pthread_self();
    threadArgs = malloc(sizeof *threadArgs);

    threadArgs->master_socket = master_socket;
    threadArgs->addressClient = addressClient;
    threadArgs->addrClientlen = addrClientlen;
    threadArgs->dA = dA;
    threadArgs->readfds = readfds;
    threadArgs->writefds = writefds;
    threadArgs->tId = tId;
    threadArgs->mtx = mtx;

    //create new thread to resolve domain and connect to server. When it's done, add all file descriptors to arrays
    if ( pthread_create( &threadForConnection, NULL, &handleThreadForConnection, (void*) threadArgs) !=0 )
        perror("thread creation failed");
    return;

}

void* handleThreadForConnection( void* args ){
    int new_server_socket , new_client_socket;
    char *message = "Connection through proxy... \r\n";
    struct ThreadArgs* tArgs;
    struct addrinfo hints, *servinfo, *p;
    int rv;
 
    tArgs = (struct ThreadArgs *)args;

    if ((new_client_socket = accept(tArgs->master_socket, (struct sockaddr *)tArgs->addressClient, (socklen_t*)tArgs->addrClientlen))<0){
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    //in case of receiving an IP, the creation of this thread is unnecessary. Check later.
    if ((rv = getaddrinfo("localhost", "pop3", &hints, &servinfo)) != 0) {
        perror("DNS resolution failed");
        exit(EXIT_FAILURE);
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((new_server_socket = socket(p->ai_family, p->ai_socktype, 0)) <= 0) {
            perror("server socket failed");
            continue;
        }

        if (connect(new_server_socket, p->ai_addr, p->ai_addrlen) < 0) {
            perror("connect failed");
            close(new_server_socket);
            continue;
        }
        // if we get here, we must have connected successfully
        break; 
    }

    if (p == NULL) {
        // looped off the end of the list with no connection
        perror("all connects failed");
    }
             
    //send new connection greeting message
    if( write(new_client_socket, message, strlen(message)) != strlen(message) ){
        perror("write greeting message failed");
    }
             
    pthread_mutex_lock(tArgs->mtx);             
    //add new socket to array of sockets
    for ( int i = 0; i < MAXCLIENTS; i++){
        //if position is empty
        if( tArgs->dA->client_sockets_read[i] == 0 ){
            tArgs->dA->client_sockets_read[i] = new_client_socket;
            tArgs->dA->server_sockets_read[i] = new_server_socket;
            break;
        }      
    }
    pthread_mutex_unlock(tArgs->mtx);

    printf("New connection: Client socket fd: %d , ip: %s , port: %d\n" , new_client_socket , inet_ntoa(tArgs->addressClient->sin_addr) , ntohs(tArgs->addressClient->sin_port));
    printf("                Server socket fd: %d , ip: %s , port: %d\n" , new_server_socket , "127.0.0.1" , 110);
    
    //send signal to main thread to let it know that the domain name has been resolved and the connection has been made 
    pthread_kill( tArgs->tId, SIGUSR1);
    free(tArgs);
    pthread_exit(0);
    return NULL;
}

void handleIOOperations(struct DescriptorsArrays* descriptorsArrays, fd_set* readfds, fd_set* writefds, char buffer[MAXCLIENTS][BUFSIZE], struct sockaddr_in* addressClient, int* addrClientlen){
    int i, j, sdc, sds;
    for (i = 0; i < MAXCLIENTS; i++){
        sdc = descriptorsArrays->client_sockets_read[i];
        sds = descriptorsArrays->server_sockets_read[i];

        //reading requests from client  
        if (FD_ISSET( sdc , readfds)){
            //resets buffer
            for( j = 0 ; j < BUFSIZE ; j++ ){
                buffer[i][j] = '\0';
            }
            //Handle client conection
            //Here it calls the parser function and analyse the requests. For now it does nothing, sends command without changing it.
            int handleResponse = handleClient(sdc, buffer[i], BUFSIZE);
            //int handleResponse = read( sdc , buffer[i], BUFSIZE);
            if( handleResponse == 0 ){
                //Somebody disconnected , get his details and print
                getpeername(sdc , (struct sockaddr*)addressClient , (socklen_t*)addrClientlen);
                printf("Host disconnected , ip %s , port %d \n" , inet_ntoa((*addressClient).sin_addr) , ntohs((*addressClient).sin_port));
                      
                //Close the socket and mark as 0 in list for reuse
                close( sdc );
                close( sds );
                descriptorsArrays->client_sockets_read[i] = 0;
                descriptorsArrays->server_sockets_read[i] = 0;
            }
            else if(handleResponse < 0){
                perror("read failed");
            }
            else{
                printf("Reading something from client:\n");
                printf("%s\n", buffer[i] );
                        descriptorsArrays->server_sockets_write[i] = descriptorsArrays->server_sockets_read[i]; 
            }
        }

        //reading responses from server
        if (FD_ISSET( sds , readfds)){
            //resets buffer
            for( j = 0 ; j < BUFSIZE ; j++ ){
                buffer[i][j] = '\0';
            }
            //Handle server conection
            //Here we should call the parser function and analyse the responses
            int handleResponse = read( sds , buffer[i], BUFSIZE);
            if( handleResponse == 0 ){
                //Somebody disconnected , get his details and print
                getpeername(sdc , (struct sockaddr*)addressClient , (socklen_t*)addrClientlen);
                printf("Server disconnected\n");
                      
                //Close the socket and mark as 0 in list for reuse
                close( sdc );
                close( sds );
                descriptorsArrays->client_sockets_read[i] = 0;
                descriptorsArrays->server_sockets_read[i] = 0;
            }
            else if(handleResponse < 0){
                perror("read failed");
            }
            else{
                printf("Reading something from server:\n");
                printf("%s\n", buffer[i] );
                descriptorsArrays->client_sockets_write[i] = descriptorsArrays->client_sockets_read[i]; 
            }
        }

        sdc = descriptorsArrays->client_sockets_write[i];
        sds = descriptorsArrays->server_sockets_write[i];

        //writing to client
        if (FD_ISSET( sdc , writefds )){
            if ( write( sdc , buffer[i], strlen(buffer[i])) != strlen(buffer[i])){
                perror("write failed");       
            }
            for( j = 0 ; j < BUFSIZE ; j++ ){
                buffer[i][j] = '\0';
            }   
            descriptorsArrays->client_sockets_write[i] = 0;

        }
        //writing to server
        if (FD_ISSET( sds , writefds )){
            if ( write( sds , buffer[i], strlen(buffer[i])) != strlen(buffer[i])){
                perror("write failed");       
            }
            for( j = 0 ; j < BUFSIZE ; j++ ){
                buffer[i][j] = '\0';
            }
            descriptorsArrays->server_sockets_write[i] = 0;
        }
    }    
}