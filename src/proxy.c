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
#include <sys/signal.h>
#include <pthread.h>
#include "proxyUtilities.h"

/**
 * Received signal function. Does nothing...
 */
static void wake_handler(const int signal) {
    printf("SIGNAL RECEIVED. DNS resolution and connection succeeded.\n\n");
    return;
}

int main(int argc , char *argv[])
{
    int opt = TRUE;
    int master_socket , new_client_socket , new_server_socket , addrClientlen ;
    int activity ,valread , sdc , sds , max_sd , aux_max_sd , i , j;
    struct sockaddr_in addressClient;
    struct DescriptorsArrays descriptorsArrays;
    char buffer[MAXCLIENTS][BUFSIZE];
    //set of socket descriptors
    fd_set readfds, writefds;
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    sigset_t emptyset, blockset;
    struct sigaction act = {
        .sa_handler = wake_handler,
    };

    // Settings for signals actions
    // Block SIGUSR1 (signal with no specific use)
    sigemptyset(&blockset);  
    sigaddset(&blockset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &blockset, NULL);
    sigaction(SIGUSR1, &act, NULL);
    sigemptyset(&emptyset);

    //Initialize arrays
    initialiseArrays( &descriptorsArrays, buffer );

    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0){
        perror("master socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ){
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
  
    //type of socket for client and server
    setSocketType( &addressClient );
    addrClientlen = sizeof(addressClient);
      
    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&addressClient, sizeof(addressClient))<0){
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
         
    //try to specify maximum of MAXCLIENTS pending connections for the master socket
    if (listen(master_socket, MAXCLIENTS) < 0){
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Listening on port %d, waiting for connections...\n\n", PORT);
     
    while(TRUE){
        //clear the socket sets
        pthread_mutex_lock(&mtx);
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
  
        //add master socket to sets
        FD_SET(master_socket, &readfds);
        FD_SET(master_socket, &writefds);
        max_sd = master_socket;

        //adds child sockets to set         
        //returns the highest file descriptor number, needed for the pselect function
        aux_max_sd = addDescriptors( &descriptorsArrays, &readfds, &writefds);
        pthread_mutex_unlock(&mtx);
        if( aux_max_sd > max_sd )
            max_sd = aux_max_sd;
  
        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = pselect( max_sd + 1 , &readfds , &writefds , NULL , NULL, &emptyset);
    
        if ((activity < 0) && (errno!=EINTR)){
            printf("select error");
        }
          
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)){
            handleNewConnection(master_socket, &addressClient, &addrClientlen , &descriptorsArrays,  &readfds, &writefds , &mtx );
        }    
        //else it's some IO operation on some other socket
        else{
            handleIOOperations(&descriptorsArrays, &readfds, &writefds, buffer, &addressClient, &addrClientlen);     
        }
    }
    return 0;
}