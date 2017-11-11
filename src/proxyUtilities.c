#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h> 
#include <arpa/inet.h>   
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h> 
#include <sys/select.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "proxyUtilities.h"
#include "parsers.h"
#include "buffer.h"

pthread_t               threadForConnection;
extern struct Metrics   metrics;
extern struct Settings  settings;
extern FILE             *proxy_log;
extern FILE             *retrieved_mail;
extern FILE             *transformed_mail;
extern int              **pipes_fd;

void setNullPointers(struct DescriptorsArrays* descriptors_arrays, int **conf_sockets_array, struct buffer ****buffers, struct InfoClient** info_clients) {
    descriptors_arrays->client_sockets_read          = NULL;
    descriptors_arrays->client_sockets_write         = NULL;
    descriptors_arrays->server_sockets_read          = NULL;
    descriptors_arrays->server_sockets_write         = NULL;
    descriptors_arrays->client_commands_to_process   = NULL;
    descriptors_arrays->server_commands_to_process   = NULL;
    descriptors_arrays->server_sends_closure         = NULL;
    descriptors_arrays->client_needs_closure         = NULL;
    *conf_sockets_array                              = NULL;
    *buffers                                         = NULL;
    *info_clients                                    = NULL;
}

void setDefaultMetrics() {
    metrics.concurrent_connections  = 0;
    metrics.cc_on                   = 0;
    metrics.historical_accesses     = 0;
    metrics.ha_on                   = 0;
    metrics.transfered_bytes        = 0;
    metrics.tb_on                   = 0;
}

void setDefaultSettings() {
    settings.origin_server = "127.0.0.1";
    settings.error_file = "/dev/null";
    settings.pop3_address = NULL;                                 //All addresses for default
    settings.management_address = "127.0.0.1";
    settings.replacement_message = "Parte reemplazada...";
    settings.censurable = NULL;
    settings.management_port = 9090;
    settings.pop3_port = 1110;
    settings.origin_port = 110;
    settings.cmd = NULL;
    settings.version = "0.7.0\n";
}

void readArguments(int argc, char *argv[]) {
    FILE    *fp;
    int     option                  = 0;
    int     message_multiple_lines  = 0;
    int     error                   = 0;

    //Especificamos las opciones. Aquellas que aparecen seguidas de el
    //caracter ':' indican que requieren un argumento.

    while ((option = getopt(argc, argv,"e:hl:L:m:M:o:p:P:t:v")) != -1) {
        switch (option) {
            case 'e' : 
                settings.error_file = malloc(strlen(optarg));
                strcpy(settings.error_file, optarg);
                fp = freopen(settings.error_file, "a", stderr);
                break;
            case 'h' : 
                printf("No help available, try commands one by one... \n");
                exit(0);
                break;
            case 'l' :
                settings.pop3_address = malloc(strlen(optarg));
                strcpy(settings.pop3_address, optarg);
                break;
            case 'L' : 
                settings.management_address = malloc(strlen(optarg));
                strcpy(settings.management_address, optarg);
                break;
            case 'm' :
                if (message_multiple_lines == 0) {
                    message_multiple_lines = 1;
                    settings.replacement_message = malloc(strlen(optarg));
                    strcpy(settings.replacement_message, optarg);
                }
                else {
                    settings.replacement_message = realloc(settings.replacement_message, strlen(settings.replacement_message) + strlen("\\n") + strlen(optarg));
                    strcat(settings.replacement_message, "\n");
                    strcat(settings.replacement_message, optarg);
                }
                break;
            case 'M' :
                settings.censurable = malloc(strlen(optarg));
                strcpy(settings.censurable, optarg);
                break;
            case 'o' :
                settings.management_port = atoi(optarg);
                break;
            case 'p' :
                settings.pop3_port = atoi(optarg);
                break;
            case 'P' :
                settings.origin_port = atoi(optarg);
                break;
            case 't' :
                settings.cmd = malloc(strlen(optarg));
                strcpy(settings.cmd, optarg);
                break;
            case 'v' :
                printf("Proxy version: %s\n", settings.version);
                exit(0);
                break;
            default: 
                error = 1;
                break;
        }
    }

    if(argc > 1) {
        settings.origin_server = malloc(strlen(argv[argc-1]));
        strcpy(settings.origin_server, argv[argc-1]);
    }
}

void configureSocket(int* sock, int dom, int type, int prot, int level, int optname, int *optval, struct sockaddr_in *address, int *addr_len, int port, char *if_addr) {

    //create a master socket for clients and a configuration master socket
    if( (*sock = socket(dom , type , prot)) == 0) {
        perror("master socket failed");
        exit(EXIT_FAILURE);
    }
    //set master sockets to allow multiple connections
    if( setsockopt(*sock, level, optname, (char *)optval, sizeof(*optval)) < 0 ) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    //type of socket for client and config
    setSocketType( address, port, if_addr );
    *addr_len = sizeof(*address);

   
    //bind the master client socket to localhost port 1110, and the config socket to 9090
    if (bind(*sock, (struct sockaddr *)address, sizeof(*address))<0) {
        perror("bind master socket failed");
        exit(EXIT_FAILURE);
    }
         
    //try to specify maximum of MAXCLIENTS pending connections for the master sockets. CHECK MAX AMOUNT OF FD...
    if (listen(*sock, MAXCLIENTS) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
}

void setSocketType( struct sockaddr_in* client_address, int port, char *if_addr ) {
    //type of socket created for client
    client_address->sin_family      = AF_INET;
    if(if_addr == NULL)
        client_address->sin_addr.s_addr = INADDR_ANY;
    else
        client_address->sin_addr.s_addr = inet_addr(if_addr); 
    client_address->sin_port        = htons( port );
}

int addDescriptors( struct DescriptorsArrays* dA, int **conf_sockets_array, fd_set* readfds, fd_set* writefds, int n, int m) {
    int sdc, sds, conf_ds, max_sd;
    max_sd = 0;

    //Add management descriptors to read set
    for ( int j = 0 ; j < m ; j++){
        conf_ds = (*conf_sockets_array)[j];
        if (conf_ds > 0){
            FD_SET( conf_ds , readfds);
            if(conf_ds > max_sd)
                max_sd = conf_ds;
        }
    }

    //Add POP3 clients descriptors to read and write set
    for ( int i = 0 ; i < n ; i++) {
        //if valid socket descriptor then add to read list
        sdc = dA->client_sockets_read[i];
        sds = dA->server_sockets_read[i];
        if(sdc > 0) {
            FD_SET( sdc , readfds);
            if(sdc > max_sd)
                max_sd = sdc;
        }     
        if(sds > 0) {
            FD_SET( sds , readfds);
            if(sds > max_sd)
                max_sd = sds;
        }
        //if reading endpoint of second pipe is valid, then add to read list
        if(pipes_fd[i][2] > -1) {
            FD_SET( pipes_fd[i][2] , readfds);
            if(pipes_fd[i][2] > max_sd)
                max_sd = pipes_fd[i][2];   
        }
        //if valid socket descriptor then add to write list
        sdc = dA->client_sockets_write[i];
        sds = dA->server_sockets_write[i]; 
        if(sdc > 0) {
            FD_SET( sdc , writefds);             
            if(sdc > max_sd)
                max_sd = sdc;
        }
        if(sds > 0) {
            FD_SET( sds , writefds);             
            if(sds > max_sd)
                max_sd = sds;
        }
    }
    return max_sd;
}

void handleConfConnection(int conf_master_socket, struct sockaddr_in *conf_address, int *conf_addr_len, int **conf_sockets_array, int *conf_sockets_amount, pthread_mutex_t *mtx){
    int     new_conf_socket;
    int     add_fd              = 1;
    char    *message            = "Connecting to POP3 proxy for configuration... \r\n";

    if ((new_conf_socket = accept(conf_master_socket, (struct sockaddr *)conf_address, (socklen_t*)conf_addr_len))<0) {
        perror("---config--- accept failed");
        return;
        //exit(EXIT_FAILURE);
    }

    //send new connection greeting message
    if( write(new_conf_socket, message, strlen(message)) != strlen(message) ) {
        perror("---config--- write greeting message failed");
    }

    pthread_mutex_lock(mtx); 
    for ( int i = 0; i < *conf_sockets_amount ; i++){
        if( (*conf_sockets_array)[i] == 0 ){
            (*conf_sockets_array)[i] = new_conf_socket;
            add_fd                   = 0;
            break;
        }
    } 
    if(add_fd){
        *conf_sockets_array = realloc( *conf_sockets_array, ((*conf_sockets_amount)+1)*sizeof(int) );
        (*conf_sockets_amount)++;
        (*conf_sockets_array)[(*conf_sockets_amount)-1] = new_conf_socket;
    }
    pthread_mutex_unlock(mtx);

    printf("New configuration connection: Socket fd: %d , ip: %s , port: %d\n" , new_conf_socket , inet_ntoa(conf_address->sin_addr) , ntohs(conf_address->sin_port));

    fwrite("Admin connected\n", 1, 16, proxy_log);
    return;
}

void handleNewConnection(int ms, struct sockaddr_in *ca, int *cal, struct DescriptorsArrays *dA, pthread_mutex_t *mtx, int *cAm, struct buffer ****b, struct InfoClient** info_clients) {
    struct ThreadArgs*  threadArgs;
    pthread_t           tId;

    tId         = pthread_self();
    threadArgs  = malloc(sizeof *threadArgs);

    threadArgs->clients_amount  = cAm;
    threadArgs->master_socket   = ms;
    threadArgs->client_address  = ca;
    threadArgs->client_addr_len = cal;
    threadArgs->dA              = dA;
    threadArgs->tId             = tId;
    threadArgs->mtx             = mtx;
    threadArgs->buffers         = b;
    threadArgs->info_clients    = info_clients;
    //create new thread to resolve domain and connect to server. When it's done, add all file descriptors to arrays
    if ( pthread_create( &threadForConnection, NULL, &handleThreadForConnection, (void*) threadArgs) !=0 )
        perror("thread creation failed");
    return;

}

void* handleThreadForConnection(void* args) {
    char                *message                                        = "Connecting through POP3 proxy... \r\n";
    int                 new_server_socket , new_client_socket, rv, i;
    struct ThreadArgs   *tArgs;
    //struct used for resolving name, if needed
    struct addrinfo     hints, *servinfo, *p;
    // Server address structs for IPv4 or IPv6
    struct sockaddr_in  servAddr4;
    struct sockaddr_in6 servAddr6;     

    pthread_detach(pthread_self());

    tArgs = (struct ThreadArgs *)args;

    if ((new_client_socket = accept(tArgs->master_socket, (struct sockaddr *)tArgs->client_address, (socklen_t*)tArgs->client_addr_len))<0) {
        perror("accept failed");
        pthread_kill( tArgs->tId, SIGUSR1);
        pthread_exit(0);
        //exit(EXIT_FAILURE);
    }

    memset(&servAddr4, 0, sizeof(servAddr4)); // Zero out structure
    servAddr4.sin_family = AF_INET;
    memset(&servAddr6, 0, sizeof(servAddr6)); // Zero out structure
    servAddr6.sin6_family = AF_INET6;
    
    
    //IPv4
    if( inet_pton(AF_INET, settings.origin_server, &servAddr4.sin_addr.s_addr) == 1 ) {
        servAddr4.sin_port = htons(settings.origin_port);
        if ((new_server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) <= 0) {
            perror("server socket failed");
            close(new_client_socket);
            pthread_kill( tArgs->tId, SIGUSR1);
            pthread_exit(0);
            //exit(EXIT_FAILURE);
            return NULL;
        }
        if (connect(new_server_socket, (struct sockaddr *) &servAddr4, sizeof(servAddr4)) < 0) {
            perror("connect failed");
            close(new_client_socket);
            close(new_server_socket);
            pthread_kill( tArgs->tId, SIGUSR1);
            pthread_exit(0);
            //exit(EXIT_FAILURE);
        } 
    }
    //IPv6
    else if ( inet_pton(AF_INET6, settings.origin_server, &servAddr6.sin6_addr.s6_addr) == 1 ) {
        servAddr6.sin6_port = htons(settings.origin_port);
        if ((new_server_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) <= 0) {
            perror("server socket failed");
            close(new_client_socket);
            pthread_kill( tArgs->tId, SIGUSR1);
            pthread_exit(0);
            //exit(EXIT_FAILURE);
            return NULL;
        }
        if (connect(new_server_socket, (struct sockaddr *) &servAddr6, sizeof(servAddr6)) < 0) {
            perror("connect failed");
            close(new_client_socket);
            close(new_server_socket);
            pthread_kill( tArgs->tId, SIGUSR1);
            pthread_exit(0);
            //exit(EXIT_FAILURE);
        } 
    }
    //domain name
    else {
        memset(&hints, 0, sizeof hints);
        hints.ai_family     = AF_INET;
        hints.ai_socktype   = SOCK_STREAM;

        //in case of receiving an IP, the creation of this thread is unnecessary. Check later.
        //change localhost and pop3 strings.
        if ((rv = getaddrinfo(settings.origin_server, "pop3", &hints, &servinfo)) != 0) {
            perror("DNS resolution failed");
            close(new_client_socket);
            pthread_kill( tArgs->tId, SIGUSR1);
            pthread_exit(0);
        }

        // loop through all the results and connect to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((new_server_socket = socket(p->ai_family, p->ai_socktype, 0)) <= 0) {
                perror("server socket failed");
                continue;
            }

            if (connect(new_server_socket, p->ai_addr, p->ai_addrlen) < 0) {
                perror("connect failed");
                continue;
            }
            // if we get here, we must have connected successfully
            break; 
        }
        if (p == NULL) {
            // looped off the end of the list with no connection
            perror("all connects failed");
        }
    }
             
    //send new connection greeting message
    if( write(new_client_socket, message, strlen(message)) != strlen(message) ) {
        perror("write greeting message failed");
    }
             
    pthread_mutex_lock(tArgs->mtx); 
    //add new socket to array of sockets if place is available
    for ( i = 0; i < *(tArgs->clients_amount); i++) {
        //if position is empty
        if( tArgs->dA->client_sockets_read[i] == 0 ){
            break;
        }      
    }
    //if there are no places available, realloc and add at the end
    if(i == *(tArgs->clients_amount)) {
        reserveSpace(tArgs);
    }

    tArgs->dA->client_sockets_read[i]               = new_client_socket;
    tArgs->dA->server_sockets_read[i]               = new_server_socket;
    tArgs->dA->client_sockets_write[i]              = 0;
    tArgs->dA->server_sockets_write[i]              = 0;
    tArgs->dA->client_commands_to_process[i]        = 0;
    tArgs->dA->server_commands_to_process[i]        = 0;
    tArgs->dA->server_sends_closure[i]              = 0;
    tArgs->dA->client_needs_closure[i]              = 0;
    (*(tArgs->info_clients))[i].retr_invoked        = 0;
    (*(tArgs->info_clients))[i].available_to_write  = 0;
    (*(tArgs->info_clients))[i].user_name           = NULL;
    for( int j = 0; j < 4; j++ ) {
        pipes_fd[i][j] = -1;
    }

    pthread_mutex_unlock(tArgs->mtx);

    printf("New connection: Client socket fd: %d , ip: %s , port: %d\n" , new_client_socket , inet_ntoa(tArgs->client_address->sin_addr) , ntohs(tArgs->client_address->sin_port));
    printf("                Server socket fd: %d , ip: %s , port: %d\n" , new_server_socket , settings.origin_server , settings.origin_port);
    metrics.concurrent_connections++;
    metrics.historical_accesses++;
    
    //send signal to main thread to let it know that the domain name has been resolved and the connection has been made 
    pthread_kill( tArgs->tId, SIGUSR1);
    pthread_exit(0);
}

//Reserves space for every structure requiered...
void reserveSpace(struct ThreadArgs * tArgs){
    char            *aux_buf_c                        = NULL;
    char            *aux_buf_s                        = NULL;
    struct buffer   *client_buffer, *server_buffer;

    tArgs->dA->client_sockets_read          = realloc(tArgs->dA->client_sockets_read, (*(tArgs->clients_amount)+1)*sizeof(int));
    tArgs->dA->client_sockets_write         = realloc(tArgs->dA->client_sockets_write, (*(tArgs->clients_amount)+1)*sizeof(int));
    tArgs->dA->server_sockets_read          = realloc(tArgs->dA->server_sockets_read, (*(tArgs->clients_amount)+1)*sizeof(int));
    tArgs->dA->server_sockets_write         = realloc(tArgs->dA->server_sockets_write, (*(tArgs->clients_amount)+1)*sizeof(int));
    tArgs->dA->client_commands_to_process   = realloc(tArgs->dA->client_commands_to_process, (*(tArgs->clients_amount)+1)*sizeof(int));
    tArgs->dA->server_commands_to_process   = realloc(tArgs->dA->server_commands_to_process, (*(tArgs->clients_amount)+1)*sizeof(int));
    tArgs->dA->server_sends_closure         = realloc(tArgs->dA->server_sends_closure, (*(tArgs->clients_amount)+1)*sizeof(int));
    tArgs->dA->client_needs_closure         = realloc(tArgs->dA->client_needs_closure, (*(tArgs->clients_amount)+1)*sizeof(int));
    *(tArgs->info_clients)                  = realloc(*(tArgs->info_clients), (*(tArgs->clients_amount)+1)*sizeof(struct InfoClient));
    *(tArgs->buffers)                       = realloc(*(tArgs->buffers), (*(tArgs->clients_amount)+1)*sizeof(struct buffer**));
    pipes_fd                                = realloc(pipes_fd, (*(tArgs->clients_amount)+1)*sizeof(int*));

    (*(tArgs->clients_amount))++;
    (*(tArgs->buffers))[*(tArgs->clients_amount)-1] = malloc(2*sizeof(struct buffer*));
    pipes_fd[*(tArgs->clients_amount)-1]            = malloc(4*sizeof(int));

    client_buffer = malloc(sizeof(struct buffer));
    server_buffer = malloc(sizeof(struct buffer));

    //Couldn't initialize with buffer_init -> Hard coded init. Check in the future.
    /*aux_buf_c = malloc(BUFSIZE*sizeof(char));
    aux_buf_s = malloc(BUFSIZE*sizeof(char));

    for(int j = 0; j < BUFSIZE; j++) {
        aux_buf_c[j] = '\0';
        aux_buf_s[j] = '\0';
    }

    buffer_init(&client_buffer, BUFSIZE, aux_buf_c);
    buffer_init(&server_buffer, BUFSIZE, aux_buf_s);
    */ 

    client_buffer->data = malloc(BUFSIZE*sizeof(char));
    server_buffer->data = malloc(BUFSIZE*sizeof(char));    

    for(int j = 0; j < BUFSIZE; j++) {
        (client_buffer->data)[j] = '\0';
        (server_buffer->data)[j] = '\0';
    }
    buffer_reset(client_buffer);
    client_buffer->limit = client_buffer->data + BUFSIZE;
    buffer_reset(server_buffer);
    server_buffer->limit = server_buffer->data + BUFSIZE;

    (*(tArgs->buffers))[*(tArgs->clients_amount)-1][0] = client_buffer;
    (*(tArgs->buffers))[*(tArgs->clients_amount)-1][1] = server_buffer;

}

void handleIOOperations(struct DescriptorsArrays *descriptors_arrays, int **conf_sockets_array, fd_set *readfds, fd_set *writefds, struct buffer ****b, struct sockaddr_in *client_address, struct sockaddr_in *conf_address, int *client_addr_len, int *conf_addr_len, int clients_amount, int conf_sockets_amount, struct InfoClient** info_clients, int* master_socket, int* conf_master_socket, int *opt_ms, int *opt_cms) {
    int             i, str_size;
    char            aux_buf[BUFSIZE];
    struct buffer   ***buffer           = *b;

    //handle configuration admins
    for (int k = 0; k < conf_sockets_amount; k++ ){
        if (FD_ISSET( (*conf_sockets_array)[k] , readfds ))
            handleManagementRequests(k, conf_sockets_array, conf_address, conf_addr_len, client_address, client_addr_len, master_socket, conf_master_socket, opt_ms, opt_cms);
    }

    //handle pop3 clients
    for (i = 0; i < clients_amount; i++) {
        
        //writing to client
        if (FD_ISSET( descriptors_arrays->client_sockets_write[i] , writefds )) {
            writeToClient(i, descriptors_arrays, b); 
        }
        //writing to server
        if (FD_ISSET( descriptors_arrays->server_sockets_write[i] , writefds ) && ( (*info_clients)[i].pipeline_supported == 1 || !FD_ISSET(descriptors_arrays->server_sockets_read[i], readfds) ) ) {
            writeToServer(i, descriptors_arrays, b, info_clients);
        }

        //reading requests from client  
        if ( FD_ISSET(descriptors_arrays->client_sockets_read[i] , readfds) && (*info_clients)[i].available_to_write == 1 ) { 
            readFromClient(i, descriptors_arrays, b, client_address, client_addr_len);
            
        }

        //reading responses from server
        if ( FD_ISSET(descriptors_arrays->server_sockets_read[i] , readfds) ) {

            //If RETR was invoked, then its reply is redirected to a file instead of the client.
            if ((*info_clients)[i].retr_invoked == 1 ){
                pid_t pid, parent_pid;

                //So that the parent knows he'll have a child (to write into the pipe instead of client)
                (descriptors_arrays->server_commands_to_process[i]) = 1;

                if(pipe(pipes_fd[i]) == -1) {
                    perror("pipe 1 failed");
                    return;
                }
                if(pipe(pipes_fd[i]+2) == -1) {
                    perror("pipe 2 failed");
                    return;   
                }

                printf("\nFORK\n");
                parent_pid = getpid();
                pid = fork();
                if (pid ==  0) {
                    // This is the child process.
                    //Sends errors to specific file instead of stderr. Don't know if it's needed when parent process already did this
                    freopen(settings.error_file, "a", stderr);
                    setEnvironmentVars(i, (*info_clients)[i].user_name);                    
                    handleChildProcess(i);
                    printf("Child terminated\n\n");
                    exit(0);
                }
                else if (pid < (pid_t) 0) {
                    perror ("fork failed");
                }
                else {
                    // This is the parent process
                    //close parent read endpoint in first pipe and parent write endpoint in second pipe
                    close(pipes_fd[i][0]);
                    pipes_fd[i][0] = -1;
                    close(pipes_fd[i][3]);
                    pipes_fd[i][3] = -1;
                    (*info_clients)[i].retr_invoked = 0 ;
                }
                printf("Parent process keeps going\n\n");
            }
            //Get response from server and send it to client if there was no RETR, or to child process otherwise
            else {
                readFromServer(i, descriptors_arrays, b, client_address, client_addr_len, info_clients);
            }
        }

        //If child process has data to send to client, then write it to its buffer
        if( FD_ISSET(pipes_fd[i][2], readfds) ){
            readFromPipe(i, descriptors_arrays, b);
               
        }
        //If server stopped returning an answer (this is usefull for mails), close pipe
        //Look for a better way to do this
        int count;
        ioctl(descriptors_arrays->server_sockets_read[i], FIONREAD, &count);
        if ( (count <= 0 && descriptors_arrays->server_commands_to_process[i] == 1) ){
                printf("CLOSE FIRST PIPE\n\n");
                descriptors_arrays->server_commands_to_process[i] = 0;
                
                //close parent write endpoint in first pipe
                close(pipes_fd[i][1]);
                pipes_fd[i][1] = -1;
        }
    } 
}

void handleManagementRequests(int k, int **conf_sockets_array, struct sockaddr_in *conf_address, int *conf_addr_len, struct sockaddr_in *client_address, int *client_addr_len, int *master_socket, int *conf_master_socket, int *opt_ms, int *opt_cms) {
    enum    Response                { ERROR = 0, OK, GCC, GHA, GTB, VN, PP, MP };
    char    aux_conf_buf[BUFSIZE];
    int     conf_ds, resp;

    conf_ds = (*conf_sockets_array)[k];
            const char  *ok_msg  = "-OK-\n";
            const char  *er_msg  = "-ERROR-\n";
            const char  *cc_msg  = "-Concurrent connections-\n";
            const char  *ha_msg  = "-Historical accesses-\n";
            const char  *tb_msg  = "-Transfered Bytes-\n";
            const char  *cc_off  = "-Concurrent connections metrics are off-\n";
            const char  *ha_off  = "-Historical accesses metrics are off-\n";
            const char  *tb_off  = "-Transfered Bytes metrics are off-\n";
            //Max number must fit in array, plus 1 space for \n
            //for int 
            char        number_of_cc[11];
            char        number_of_ha[11];
            //for unsigned long 
            char        number_of_tb[21];

            for(int m = 0; m < 11; m++){
                number_of_cc[m] = '\0';
                number_of_ha[m] = '\0';
            }
            for(int n = 0; n < 21; n++)
                number_of_tb[n] = '\0';

            printf("---config--- CONNECTION %d, ENTER READ\n", k);
            for(int  l = 0 ; l < BUFSIZE ; l++ ) {
                aux_conf_buf[l] = '\0';
            }
            int handleResponse = read( conf_ds , aux_conf_buf, BUFSIZE);   
            
            if( handleResponse == 0 ) {
                //Somebody disconnected , get his details and print
                getpeername(conf_ds , (struct sockaddr*)conf_address , (socklen_t*)conf_addr_len);
                printf("---config--- Config Host disconnected , ip %s , port %d \n" , inet_ntoa((*conf_address).sin_addr) , ntohs((*conf_address).sin_port));
                      
                //Close the socket and mark as 0 in list for reuse, or
                printf("---config--- CONNECTION %d, ADMIN CLOSES\n", k);
                close( conf_ds );
                (*conf_sockets_array)[k] = 0;
                fwrite("Admin disconnected\n", 1, 19, proxy_log);
            }
            else if(handleResponse < 0) {
                perror("---config--- read failed");
            }
            else {
                printf("---config--- Read something from admin\n");
                //For now, it only echoes.
                //We need to parse aux_conf_buf content and apply the configuration
                resp = parseConfigCommand(aux_conf_buf);
                switch (resp) {
                case ERROR: if ( write( conf_ds , er_msg, strlen(er_msg) ) != strlen(er_msg) ) {
                                perror("---config--- write failed"); 
                            }
                            break;   
                case OK:    if ( write( conf_ds , ok_msg, strlen(ok_msg) ) != strlen(ok_msg) ) {
                                perror("---config--- write failed"); 
                            }
                            break;
                case GCC:   if(metrics.cc_on == 1) {
                                if ( write( conf_ds , cc_msg, strlen(cc_msg) ) != strlen(cc_msg) )
                                    perror("---config--- write failed"); 
                                snprintf( number_of_cc, 11, "%d", metrics.concurrent_connections );
                                number_of_cc[strlen(number_of_cc)] = '\n';
                                printf("%s\n", number_of_cc);
                                if ( write( conf_ds , number_of_cc, strlen(number_of_cc) ) != strlen(number_of_cc) )
                                    perror("---config--- write failed"); 
                            }
                            else{
                                if ( write( conf_ds , cc_off, strlen(cc_off) ) != strlen(cc_off) )
                                    perror("---config--- write failed"); 
                            }
                            break;
                case GHA:   if(metrics.ha_on == 1) {
                                if ( write( conf_ds , ha_msg, strlen(ha_msg) ) != strlen(ha_msg) )
                                    perror("---config--- write failed"); 
                                snprintf( number_of_ha, 11, "%d", metrics.historical_accesses );
                                number_of_ha[strlen(number_of_ha)] = '\n';
                                if ( write( conf_ds , number_of_ha, strlen(number_of_ha) ) != strlen(number_of_ha) )
                                    perror("---config--- write failed"); 
                            }
                            else {
                                if ( write( conf_ds , ha_off, strlen(ha_off) ) != strlen(ha_off) )
                                    perror("---config--- write failed");    
                            }
                            break;
                case GTB:   if(metrics.tb_on == 1) {
                                if ( write( conf_ds , tb_msg, strlen(tb_msg) ) != strlen(tb_msg) )
                                    perror("---config--- write failed"); 
                                snprintf( number_of_tb, 21, "%lu", metrics.transfered_bytes );
                                number_of_tb[strlen(number_of_tb)] = '\n';
                                if ( write( conf_ds , number_of_tb, strlen(number_of_tb) ) != strlen(number_of_tb) )
                                    perror("---config--- write failed"); 
                            }
                            else {
                                if ( write( conf_ds , tb_off, strlen(tb_off) ) != strlen(tb_off) )
                                    perror("---config--- write failed");
                            }
                            break;
                case VN:    if ( write( conf_ds , settings.version, strlen(settings.version) ) != strlen(settings.version) ) {
                                perror("---config--- write failed"); 
                            }
                            break;
                case PP:    close(*master_socket);
                            configureSocket(master_socket, AF_INET, SOCK_STREAM, IPPROTO_TCP, SOL_SOCKET, SO_REUSEADDR, opt_ms, client_address, client_addr_len, settings.pop3_port, settings.pop3_address);
                            if ( write( conf_ds , ok_msg, strlen(ok_msg) ) != strlen(ok_msg) ) {
                                perror("---config--- write failed"); 
                            }
                            break;
                case MP:    close(*conf_master_socket);
                            configureSocket(conf_master_socket, PF_INET, SOCK_STREAM, IPPROTO_SCTP, SOL_SOCKET, SO_REUSEADDR, opt_cms, conf_address, conf_addr_len, settings.management_port, settings.management_address);
                            if ( write( conf_ds , ok_msg, strlen(ok_msg) ) != strlen(ok_msg) ) {
                                perror("---config--- write failed"); 
                            }
                            break;
                default:    break;
                }
            }

}

void writeToClient(int i, struct DescriptorsArrays *descriptors_arrays, struct buffer ****b){
    char            aux_buf[BUFSIZE+1];
    struct buffer   ***buffer         = *b;

    printf("CONNECTION %d, ENTER WRITE TO CLIENT\n", i);
    for( int j = 0 ; j <= BUFSIZE ; j++ ) {
        aux_buf[j] = '\0';
    }
    size_t  rbytes  = 0;
    uint8_t *ptr    = buffer_read_ptr(buffer[i][1], &rbytes);
    memcpy(aux_buf,ptr,strlen(ptr));

    //in case of reading an entire command, add space for '\0'
    if( (buffer[i][1]->read + rbytes) < buffer[i][1]->limit ){
        rbytes++;
    }

    buffer_read_adv(buffer[i][1], rbytes);
    if ( write( descriptors_arrays->client_sockets_write[i], aux_buf, strlen(aux_buf)) != strlen(aux_buf)) {
        perror("write failed");       
    }  
    if ( !buffer_can_read(buffer[i][1]) ){
        descriptors_arrays->client_sockets_write[i] = 0;
    }

    //if server has ended and pipe had something left to send and now it has finished too
    if(descriptors_arrays->server_sends_closure[i] == 1) {
        close( descriptors_arrays->client_sockets_read[i] );
        descriptors_arrays->client_sockets_read[i] = 0;
        buffer_reset(buffer[i][1]);
    }
}

void readFromClient(int i, struct DescriptorsArrays *descriptors_arrays, struct buffer ****b, struct sockaddr_in *client_address, int *client_addr_len) {
    int             str_size;
    char            aux_buf[BUFSIZE];
    struct buffer   ***buffer           = *b;    
 
    printf("CONNECTION %d, ENTER READ FROM CLIENT\n", i);
    for( int j = 0 ; j < BUFSIZE ; j++ ) {
        aux_buf[j] = '\0';
    }
    //Handle client conection
    int handleResponse = read( descriptors_arrays->client_sockets_read[i] , aux_buf, size_to_write(buffer[i][1]));
    if( handleResponse == 0 ) {
        //Somebody disconnected , get his details and print
        getpeername(descriptors_arrays->client_sockets_read[i] , (struct sockaddr*)client_address , (socklen_t*)client_addr_len);
        printf("Host disconnected , ip %s , port %d \n" , inet_ntoa((*client_address).sin_addr) , ntohs((*client_address).sin_port));
                      
        //Close the socket and mark as 0 in list for reuse
        printf("CONNECTION %d, CLIENT CLOSES\n", i);
        close( descriptors_arrays->client_sockets_read[i] );
        close( descriptors_arrays->server_sockets_read[i] );
        if(metrics.concurrent_connections > 0)
            metrics.concurrent_connections--;
        descriptors_arrays->client_sockets_read[i] = 0;
        descriptors_arrays->server_sockets_read[i] = 0;
        buffer_reset(buffer[i][0]);
        buffer_reset(buffer[i][1]);
        fwrite("Client disconnected\n", 1, 20, proxy_log);
    }
    else if(handleResponse < 0) {
        perror("read failed");
    }
    else {
        printf("Read something from client\n");
        str_size = 0;
        for( int j = 0; j < BUFSIZE; j++){
            if(aux_buf[j]!='\0'){
                str_size++;
            }
            else break;
        }

        //write into server buffer
        size_t  wbytes  = 0;
        uint8_t *ptr    = buffer_write_ptr(buffer[i][0], &wbytes);

        if( wbytes < str_size ){
            perror("no space in buffer to read");
        }
        memcpy(ptr, aux_buf, str_size);
        buffer_write_adv(buffer[i][0], str_size);
        (descriptors_arrays->client_commands_to_process[i]) = 1;
        descriptors_arrays->server_sockets_write[i] = descriptors_arrays->server_sockets_read[i]; 
    }
}

void writeToServer(int i, struct DescriptorsArrays *descriptors_arrays, struct buffer ****b, struct InfoClient** info_clients) {
    enum Command    { USER = 0, RETR, CAPA, OTHER };
    char            aux_buf[BUFSIZE];
    int             cmd, keep_sending;
    struct buffer   ***buffer                         = *b;

    printf("CONNECTION %d, ENTER WRITE TO SERVER\n", i);
    for( int j = 0 ; j < BUFSIZE ; j++ ) {
        aux_buf[j] = '\0';
    }
    size_t  rbytes        = 0;
    size_t  total_rbytes  = 0;
    int     first_cmd     = 1;
    //If server does not support pipelining, sends one command at a time
    //buffer_read_ptr_for_client reads only one command
    //If it does, iterate and send commands (this is to analyse if there is a retr)            
    keep_sending = 1;
    do {            
        uint8_t *ptr    = buffer_read_ptr_for_client(buffer[i][0], &rbytes);
        memcpy(aux_buf+total_rbytes,ptr,rbytes);

        //Analyse sending command
        cmd = parseClientCommand(aux_buf+total_rbytes);
        if( cmd == USER ){
            //set name in info_client struct
            //to see if command is 'USER name', it needs to escape the first 5 chars
            if (commandLength(aux_buf+total_rbytes) >= 6){
                (*info_clients)[i].user_name = malloc(commandLength(aux_buf+total_rbytes+5+1)*sizeof(char)); //+1 for \0
                memcpy((*info_clients)[i].user_name, aux_buf+total_rbytes+5, commandLength(aux_buf+total_rbytes+5));
                (*info_clients)[i].user_name[commandLength(aux_buf+total_rbytes+5)] = '\0';
                printf("New user: %s\n", (*info_clients)[i].user_name);
            }
        }
        else if( cmd == RETR ){
            //to see if command is 'RETR n', it needs to escape the first 6 chars (counting \n at the end)
            if (commandLength(aux_buf+total_rbytes) >= 6){
                (*info_clients)[i].retr_invoked = 1;
                printf("RETR invoked\n");
            }
        }
        else if ( cmd == CAPA && first_cmd ){
            (*info_clients)[i].capa_invoked = 1;
            printf("CAPA invoked\n");
        }

        buffer_read_adv(buffer[i][0], rbytes);
        total_rbytes += rbytes;
        first_cmd     = 0;

        if( (*info_clients)[i].pipeline_supported == 0 || !buffer_can_read(buffer[i][0]) )
            keep_sending = 0;
    
    } while ( keep_sending == 1);
            
    if ( write( descriptors_arrays->server_sockets_write[i], aux_buf, strlen(aux_buf)) != strlen(aux_buf)) {
        perror("write failed");       
    }

    if ( !buffer_can_read(buffer[i][0]) ){
        (descriptors_arrays->client_commands_to_process[i]) = 0;
        descriptors_arrays->server_sockets_write[i]         = 0;
    }
}

void handleChildProcess(int i) {
    char    child_aux_buf_in[BUFSIZE];
    char    child_aux_buf_out[BUFSIZE];
    char    conn_number[11];
    int     pread, nread, num_size;

    for( int p = 0 ; p < BUFSIZE ; p++ ) {
        child_aux_buf_in[p]     = '\0';
        child_aux_buf_out[p]    = '\0';
    }

    //close child write endpoint in first pipe
    close(pipes_fd[i][1]);
    //close child read endpoint in second pipe
    close(pipes_fd[i][2]);

    //Transformation needs to be applied
    //Replace 'nnnn' with number of client ('i') in file path
    char file_path_retr[]   = "./retr_mail_nnnn";
    char file_path_resp[]   = "./resp_mail_nnnn";

    intToString(i, conn_number);
    num_size = strlen(conn_number);
    for( int q = 15; q > 11; q--){
        if(num_size > 0) {
            file_path_retr[q] = conn_number[num_size-1];
            file_path_resp[q] = conn_number[num_size-1];
            num_size--;
        }
        else {
            file_path_retr[q] = 48;
            file_path_resp[q] = 48;
        }
    }

    retrieved_mail          = fopen(file_path_retr, "a");
    //transformed_mail        = fopen(file_path_resp, "a");
       
    printf("\nRETR invoked and child process created\n\n");

    while ( (pread = read(pipes_fd[i][0], child_aux_buf_in, BUFSIZE)) > 0 ) {
        if(child_aux_buf_in[pread-1] == '\0')
            pread--;
        fwrite(child_aux_buf_in, 1, pread, retrieved_mail);
                        
        //Testing, extern program should do something.
        //fwrite(child_aux_buf_in, 1, pread, transformed_mail);

        for( int p = 0 ; p < BUFSIZE ; p++ ) {
            child_aux_buf_in[p]     = '\0';
        }
    }
    fclose(retrieved_mail);
    //fclose(transformed_mail);

    //This is where we should call the transformation program and that program should change the files.
    if(settings.censurable != NULL)
        system("./testT");
    else if(settings.cmd != NULL) {
        char sys_command[strlen(settings.cmd)+36];    
        strcpy(sys_command, settings.cmd);
        strcat(sys_command, file_path_retr);
        strcat(sys_command, " > ");
        strcat(sys_command, file_path_resp);    
        system(sys_command);
    }
    else {
        char sys_command[37];
        strcpy(sys_command, "cp ");
        strcat(sys_command, file_path_retr);
        strcat(sys_command, " ");
        strcat(sys_command, file_path_resp);    
        system(sys_command);
    }

    //Send transformed mail to other pipe and then to client's buffer
    transformed_mail = fopen(file_path_resp, "r");
    while ((nread = fread(child_aux_buf_out, 1, BUFSIZE-1, transformed_mail)) > 0){
        if ( write( pipes_fd[i][3] , child_aux_buf_out, strlen(child_aux_buf_out)) != strlen(child_aux_buf_out) ) {
            perror("write failed");       
        }
        for( int p = 0 ; p < BUFSIZE ; p++ ) {
            child_aux_buf_out[p]     = '\0';
        }
    }
    if (ferror(transformed_mail)) {
        // deal with error
        printf("FERROR\n");
    }
    fclose(transformed_mail);
    //delete temporary files
    remove(file_path_retr);
    remove(file_path_resp);

    //close child read endpoint in first pipe
    close(pipes_fd[i][0]);
    //close child write endpoint in second pipe
    close(pipes_fd[i][3]);
}

void readFromServer(int i, struct DescriptorsArrays *descriptors_arrays, struct buffer ****b, struct sockaddr_in *client_address, int *client_addr_len, struct InfoClient** info_clients) {
    int             str_size;
    char            aux_buf[BUFSIZE];
    struct buffer   ***buffer             = *b;

    printf("CONNECTION %d, ENTER READ FROM SERVER\n", i);
    for( int j = 0 ; j < BUFSIZE ; j++ ) {
        aux_buf[j] = '\0';
    }
    //Handle server conection
    int handleResponse = read( descriptors_arrays->server_sockets_read[i] , aux_buf, size_to_write(buffer[i][1]));
    if( handleResponse == 0 ) {
        //Somebody disconnected , get his details and print
        getpeername(descriptors_arrays->client_sockets_read[i] , (struct sockaddr*)client_address , (socklen_t*)client_addr_len);
        printf("Server disconnected\n");
                      
        //Close the socket and mark as 0 in list for reuse
        printf("CONNECTION %d, SERVER CLOSES\n", i);
        close( descriptors_arrays->server_sockets_read[i] );    
        descriptors_arrays->server_sockets_read[i] = 0;
        buffer_reset(buffer[i][0]);
        if(metrics.concurrent_connections > 0)
            metrics.concurrent_connections--;
        //wait for second pipe to receive all data and tell client to close
        if(  pipes_fd[i][2] > -1 ) {
            descriptors_arrays->server_sends_closure[i] = 1;
        }
        //close client directly
        else {
            close( descriptors_arrays->client_sockets_read[i] );
            descriptors_arrays->client_sockets_read[i] = 0;
            buffer_reset(buffer[i][1]);
        }      
        fwrite("Server disconnected\n", 1, 20, proxy_log);            
    }
    else if(handleResponse < 0) {
        perror("read failed");
    }
    else {
        printf("Read something from server\n");
        metrics.transfered_bytes += (unsigned long int)handleResponse;

        if ( (*info_clients)[i].capa_invoked == 1 ) {
            if ( parseServerForCAPA(aux_buf) == 1 ){
                (*info_clients)[i].pipeline_supported = 1;
                printf("Pipelining supported\n");
            }
            else{
                (*info_clients)[i].pipeline_supported = 0;
                printf("Pipelining not supported\n");
            }
            (*info_clients)[i].capa_invoked = 0;
        }
                
        //Maybe with strlen it's enough, check last char if buf is full in that case
        str_size = 0;
        for( int j = 0; j < BUFSIZE; j++){
            if(aux_buf[j]!='\0'){
                str_size++;
            }
            else break;
        }
        if ( str_size < BUFSIZE && aux_buf[str_size] == '\0'){
            str_size++;
        }

        //write in first pipe for child process
        if((descriptors_arrays->server_commands_to_process[i]) == 1) {
            if ( write( pipes_fd[i][1] , aux_buf, str_size) != str_size ) {
                perror("write failed");       
            }
        } 
        //write directly to client buffer
        else {
            size_t  wbytes  = 0;
            uint8_t *ptr    = buffer_write_ptr(buffer[i][1], &wbytes);
            if( wbytes < str_size ){
                perror("no space in buffer to read");
            }
            memcpy(ptr, aux_buf, str_size);
            buffer_write_adv(buffer[i][1], str_size);

            descriptors_arrays->client_sockets_write[i] = descriptors_arrays->client_sockets_read[i];
            (*info_clients)[i].available_to_write = 1;
        }
    }
}

void readFromPipe(int i, struct DescriptorsArrays *descriptors_arrays, struct buffer ****b) {
    int             str_size;
    char            aux_buf[BUFSIZE];
    struct buffer   ***buffer                 = *b;

    printf("CONNECTION %d, ENTER READ FROM PIPE\n", i);
    for( int j = 0 ; j < BUFSIZE ; j++ ) {
        aux_buf[j] = '\0';
    }
    //Handle pipe conection
    int handleResponse = read( pipes_fd[i][2] , aux_buf, size_to_write(buffer[i][1]));
    if( handleResponse > 0 ) {
        printf("Read something from pipe\n");
        //Maybe with strlen it's enough, check last char if buf is full in that case
        str_size = 0;
        for( int j = 0; j < BUFSIZE; j++){
            if(aux_buf[j]!='\0'){
                str_size++;
            }
            else break;
        }
        if ( str_size < BUFSIZE && aux_buf[str_size] == '\0'){
            str_size++;
        }

        //write into client buffer
        size_t  wbytes  = 0;
        uint8_t *ptr    = buffer_write_ptr(buffer[i][1], &wbytes);
        if( wbytes < str_size ){
            perror("no space in buffer to read");
        }
        memcpy(ptr, aux_buf, str_size);
        buffer_write_adv(buffer[i][1], str_size);

        descriptors_arrays->client_sockets_write[i] = descriptors_arrays->client_sockets_read[i];
    }
    //else close second pipe
    else {
        printf("CLOSE SECOND PIPE\n");
        close(pipes_fd[i][2]);
        close(pipes_fd[i][3]);
        pipes_fd[i][2] = -1;
        pipes_fd[i][3] = -1;
    }
}

void setEnvironmentVars( int i, const char *name ) {
    char b[11];
    intToString(i, b);
    setenv("CLIENT_NUM", b, 1);
    if(settings.censurable != NULL)
        setenv("FILTER_MEDIAS", settings.censurable, 1);
    setenv("FILTER_MSG", settings.replacement_message, 1);
    setenv("POP3FILTER_VERSION", settings.version, 1);
    setenv("POP3_SERVER", settings.origin_server, 1); 
    if(name != NULL)
        setenv("POP3_USERNAME", name, 1);
}