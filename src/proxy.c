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

struct Metrics  metrics;
struct Settings settings;
FILE            *retrieved_mail;
FILE            *transformed_mail;
int             **pipes_fd;

/**
 * Received signal function to let know something happened in a file descriptor.
 * It's needed to unlock select().
 */
static void wake_handler(const int signal) {
    printf("SIGNAL RECEIVED. Connection succeeded.\n\n");
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
    int                         opt_ms                                  = 1;
    int                         opt_cms                                 = 1;
    int                         clients_amount                          = 0;
    int                         conf_sockets_amount                     = 0;
    int                         master_socket, conf_master_socket;
    int                         new_client_socket, new_server_socket; 
    int                         client_addr_len, conf_addr_len;
    int                         sdc, sds, max_sd, aux_max_sd;
    //to keep track of the sockets used for configuration
    int                         *conf_sockets_array;
    int                         activity, valread, i, j;
    //buffers for each connection
    struct buffer               ***buffers;
    //parsed information of every client
    struct InfoClient           *info_clients;
    //set of socket descriptors
    fd_set                      readfds, writefds;
    sigset_t                    emptyset, blockset;
    //structs and mutex
    pthread_mutex_t             mtx                                     = PTHREAD_MUTEX_INITIALIZER;
    struct sockaddr_in          client_address, conf_address;
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

    setNullPointers(&descriptors_arrays, &conf_sockets_array, &buffers, &info_clients);
    setDefaultMetrics();
    setDefaultSettings();

    //create a master socket for clients and a configuration master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , IPPROTO_TCP)) == 0) {
        perror("master socket failed");
        exit(EXIT_FAILURE);
    }

    //Config connection with SCTP
    if( (conf_master_socket = socket(PF_INET , SOCK_STREAM , IPPROTO_SCTP)) == 0) {
        perror("master socket failed");
        exit(EXIT_FAILURE);
    }

    //set master sockets to allow multiple connections
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt_ms, sizeof(opt_ms)) < 0 ) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    if( setsockopt(conf_master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt_cms, sizeof(opt_cms)) < 0 ) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
  
    //type of socket for client and config
    setSocketType( &client_address, settings.pop3_port );
    setSocketType( &conf_address, settings.management_port );
    client_addr_len = sizeof(client_address);
    conf_addr_len   = sizeof(conf_address);

      
    //bind the master client socket to localhost port 8888, and the config socket to 7777
    if (bind(master_socket, (struct sockaddr *)&client_address, sizeof(client_address))<0) {
        perror("bind master socket failed");
        exit(EXIT_FAILURE);
    }
    if (bind(conf_master_socket, (struct sockaddr *)&conf_address, sizeof(conf_address))<0) {
        perror("bind conf socket failed");
        exit(EXIT_FAILURE);
    }
         
    //try to specify maximum of MAXCLIENTS pending connections for the master sockets. CHECK MAX AMOUNT OF FD...
    if (listen(master_socket, MAXCLIENTS) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    if (listen(conf_master_socket, MAXCLIENTS) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Listening on port %d for POP3 clients...\n", settings.pop3_port);
    printf("Listening on port %d for configuration...\n\n", settings.management_port);
    
    while(1){
        //clear the socket sets
        pthread_mutex_lock(&mtx);
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
  
        //add master socket and conf socket to sets
        FD_SET(master_socket, &readfds);
        FD_SET(master_socket, &writefds);
        FD_SET(conf_master_socket, &readfds);

        max_sd = (master_socket > conf_master_socket) ? master_socket : conf_master_socket;

        //adds child sockets to set         
        //returns the highest file descriptor number, needed for the pselect function
        aux_max_sd = addDescriptors( &descriptors_arrays, &conf_sockets_array, &readfds, &writefds, clients_amount, conf_sockets_amount);
        pthread_mutex_unlock(&mtx);
        if(aux_max_sd > max_sd)
            max_sd = aux_max_sd;

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = pselect( max_sd + 1 , &readfds , &writefds , NULL , NULL, &emptyset);

        if ((activity < 0) && (errno!=EINTR)){
            printf("select error");
        }
                  
        //If something happened on the master socket , then its an incoming client connection
        if (FD_ISSET(master_socket, &readfds)) {        
            handleNewConnection(master_socket, &client_address, &client_addr_len , &descriptors_arrays , &mtx, &clients_amount, &buffers, &info_clients);
        }
        //If something happened on the config master socket , then its an incoming config connection
        else if (FD_ISSET(conf_master_socket, &readfds)) {        
            handleConfConnection(conf_master_socket, &conf_address, &conf_addr_len, &conf_sockets_array, &conf_sockets_amount, &mtx);
        }    
        //else it's some IO operation on some other socket
        else {
            handleIOOperations(&descriptors_arrays, &conf_sockets_array, &readfds, &writefds, &buffers, &client_address, &conf_address, &client_addr_len, &conf_addr_len, clients_amount, conf_sockets_amount, &info_clients);
        }
    }
    return 0;
}