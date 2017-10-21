#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h> 
#include <arpa/inet.h>   
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> 
#include <sys/select.h>
#include <stdbool.h>
#include <sys/signal.h>
#include <pthread.h>
#include "proxyUtilities.h"
#include "buffer.h"

/**
 * Received signal function to let know something happened in a file descriptor. Does nothing...
 */
static void wake_handler(const int signal) {
    printf("SIGNAL RECEIVED. DNS resolution and connection succeeded.\n\n");
    return;
}

/**
 * Provisory exit function.
 */
static void finalize_pgm(const int signal) {
    printf("\nEXITING PROGRAM.\n\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    //needed only in setsockopt
    int                         opt                                     = TRUE;
    int                         clients_amount                          = 0;
    int                         master_socket, new_server_socket; 
    int                         new_client_socket, client_addr_len;
    int                         sdc, sds, max_sd, aux_max_sd;
    int                         activity, valread, i, j;
    struct buffer               ***buffers;
    //set of socket descriptors
    fd_set                      readfds, writefds;
    sigset_t                    emptyset, blockset;
    //structs and mutex
    pthread_mutex_t             mtx                                     = PTHREAD_MUTEX_INITIALIZER;
    struct sockaddr_in          client_address;
    struct DescriptorsArrays    descriptors_arrays;
    struct sigaction act = {
        .sa_handler = wake_handler,
    };
    struct sigaction finalize = {
        .sa_handler = finalize_pgm,
    };

    // Settings for signals actions
    // Block SIGUSR1 (signal with no specific use)
    sigemptyset(&blockset);  
    sigaddset(&blockset, SIGUSR1);
    sigaddset(&blockset, SIGINT);
    sigprocmask(SIG_BLOCK, &blockset, NULL);
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGINT, &finalize, NULL);
    sigemptyset(&emptyset);

    //Set initial pointers to NULL
    descriptors_arrays.client_sockets_read  = NULL;
    descriptors_arrays.client_sockets_write = NULL;
    descriptors_arrays.server_sockets_read  = NULL;
    descriptors_arrays.server_sockets_write = NULL;
    buffers                                 = NULL;

    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) {
        perror("master socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
  
    //type of socket for client and server
    setSocketType( &client_address );
    client_addr_len = sizeof(client_address);
      
    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&client_address, sizeof(client_address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
         
    //try to specify maximum of MAXCLIENTS pending connections for the master socket. CHECK MAX AMOUNT OF FD...
    if (listen(master_socket, MAXCLIENTS) < 0) {
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
        aux_max_sd = addDescriptors( &descriptors_arrays, &readfds, &writefds, clients_amount);
        pthread_mutex_unlock(&mtx);
        if(aux_max_sd > max_sd)
            max_sd = aux_max_sd;

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = pselect( max_sd + 1 , &readfds , &writefds , NULL , NULL, &emptyset);
    
        if ((activity < 0) && (errno!=EINTR)){
            printf("select error");
        }
          
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) {        
            handleNewConnection(master_socket, &client_address, &client_addr_len , &descriptors_arrays,  &readfds, &writefds , &mtx, &clients_amount, &buffers);
        }    
        //else it's some IO operation on some other socket
        else {
            handleIOOperations(&descriptors_arrays, &readfds, &writefds, &buffers, &client_address, &client_addr_len, &clients_amount);
        }
    }
    return 0;
}