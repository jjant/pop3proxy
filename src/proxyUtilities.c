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
#include <sys/time.h> 
#include <sys/select.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/signal.h>
#include "proxyUtilities.h"
#include "buffer.h"

pthread_t threadForConnection;

void setSocketType( struct sockaddr_in* client_address ) {
    //type of socket created for client
    client_address->sin_family      = AF_INET;
    client_address->sin_addr.s_addr = INADDR_ANY;
    client_address->sin_port        = htons( PORT );
}

int addDescriptors( struct DescriptorsArrays* dA, fd_set* readfds, fd_set* writefds, int n) {
    int sdc, sds, max_sd;
    max_sd = 0;
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

void handleNewConnection(int ms, struct sockaddr_in *ca, int *cal, struct DescriptorsArrays *dA, fd_set *readfds, fd_set *writefds, pthread_mutex_t *mtx, int *cAm, struct buffer ****b) {
    struct ThreadArgs*  threadArgs;
    pthread_t           tId;

    tId         = pthread_self();
    threadArgs  = malloc(sizeof *threadArgs);

    threadArgs->clients_amount  = cAm;
    threadArgs->master_socket   = ms;
    threadArgs->client_address  = ca;
    threadArgs->client_addr_len = cal;
    threadArgs->dA              = dA;
    threadArgs->readfds         = readfds;
    threadArgs->writefds        = writefds;
    threadArgs->tId             = tId;
    threadArgs->mtx             = mtx;
    threadArgs->buffers         = b;
    //create new thread to resolve domain and connect to server. When it's done, add all file descriptors to arrays
    if ( pthread_create( &threadForConnection, NULL, &handleThreadForConnection, (void*) threadArgs) !=0 )
        perror("thread creation failed");
    return;

}

void* handleThreadForConnection(void* args) {
    char                *message                                    = "Connection through proxy... \r\n";
    int                 add_fd                                      = 1;
    int                 new_server_socket , new_client_socket, rv;
    struct ThreadArgs   *tArgs;
    struct addrinfo     hints, *servinfo, *p;
 
    tArgs = (struct ThreadArgs *)args;

    if ((new_client_socket = accept(tArgs->master_socket, (struct sockaddr *)tArgs->client_address, (socklen_t*)tArgs->client_addr_len))<0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family     = AF_INET;
    hints.ai_socktype   = SOCK_STREAM;

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
    if( write(new_client_socket, message, strlen(message)) != strlen(message) ) {
        perror("write greeting message failed");
    }
             
    pthread_mutex_lock(tArgs->mtx); 
    //add new socket to array of sockets if place is available
    for ( int i = 0; i < *(tArgs->clients_amount); i++) {
        //if position is empty
        if( tArgs->dA->client_sockets_read[i] == 0 ){
            tArgs->dA->client_sockets_read[i] = new_client_socket;
            tArgs->dA->server_sockets_read[i] = new_server_socket;
            add_fd                            = 0;
            break;
        }      
    }
    //if there are no places available, realloc and add at the end
    if(add_fd) {
        reserveSpace(tArgs);
        tArgs->dA->client_sockets_read[*(tArgs->clients_amount)-1]  = new_client_socket;
        tArgs->dA->server_sockets_read[*(tArgs->clients_amount)-1]  = new_server_socket;
        tArgs->dA->client_sockets_write[*(tArgs->clients_amount)-1] = 0;
        tArgs->dA->server_sockets_write[*(tArgs->clients_amount)-1] = 0;
    }    
    pthread_mutex_unlock(tArgs->mtx);

    printf("New connection: Client socket fd: %d , ip: %s , port: %d\n" , new_client_socket , inet_ntoa(tArgs->client_address->sin_addr) , ntohs(tArgs->client_address->sin_port));
    printf("                Server socket fd: %d , ip: %s , port: %d\n" , new_server_socket , "127.0.0.1" , 110);
    
    //send signal to main thread to let it know that the domain name has been resolved and the connection has been made 
    pthread_kill( tArgs->tId, SIGUSR1);
    
    if((*(tArgs->buffers))[0][1]->write == NULL)
                printf("buffers--->write NULL inside function\n");
    else
        printf("puntero write: %s\n", (*(tArgs->buffers))[0][1]->write);


    //free(tArgs);
    pthread_exit(0);
    //return NULL;
}

void reserveSpace(struct ThreadArgs * tArgs){
    char            *aux_buf_c                        = NULL;
    char            *aux_buf_s                        = NULL;
    struct buffer   *client_buffer, *server_buffer;

    tArgs->dA->client_sockets_read  = realloc(tArgs->dA->client_sockets_read, (*(tArgs->clients_amount)+1)*sizeof(int));
    tArgs->dA->client_sockets_write = realloc(tArgs->dA->client_sockets_write, (*(tArgs->clients_amount)+1)*sizeof(int));
    tArgs->dA->server_sockets_read  = realloc(tArgs->dA->server_sockets_read, (*(tArgs->clients_amount)+1)*sizeof(int));
    tArgs->dA->server_sockets_write = realloc(tArgs->dA->server_sockets_write, (*(tArgs->clients_amount)+1)*sizeof(int));
    *(tArgs->buffers)                  = realloc(*(tArgs->buffers), (*(tArgs->clients_amount)+1)*sizeof(struct buffer**));
    (*(tArgs->clients_amount))++;
    (*(tArgs->buffers))[*(tArgs->clients_amount)-1] = malloc(2*sizeof(struct buffer*));

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

void handleIOOperations(struct DescriptorsArrays *descriptors_arrays, fd_set *readfds, fd_set *writefds, struct buffer ****b, struct sockaddr_in *client_address, int *client_addr_len, int *clients_amount) {
    int             i, j, sdc, sds, str_size;
    char            aux_buf[BUFSIZE];
    struct buffer   ***buffer                   = *b;

    for (i = 0; i < *clients_amount; i++) {
        
        sdc = descriptors_arrays->client_sockets_write[i];
        sds = descriptors_arrays->server_sockets_write[i];

        //writing to client
        if (FD_ISSET( sdc , writefds )) {
            printf("CONEXIÓN %d, ENTER WRITE TO CLIENT\n", i);
            for( j = 0 ; j < BUFSIZE ; j++ ) {
                aux_buf[j] = '\0';
            }
            size_t  rbytes  = 0;
            uint8_t *ptr    = buffer_read_ptr(buffer[i][1], &rbytes);
            memcpy(aux_buf,ptr,rbytes);
            if(strlen(aux_buf) > BUFSIZE){
                aux_buf[BUFSIZE] = '\0';
            }

            if( (buffer[i][1]->read + rbytes) < buffer[i][1]->limit ){
                rbytes++;
            }

            buffer_read_adv(buffer[i][1], rbytes);
            if ( write( sdc , aux_buf, strlen(aux_buf)) != strlen(aux_buf)) {
                perror("write failed");       
            }  
            descriptors_arrays->client_sockets_write[i] = 0;
        }
        //writing to server
        if (FD_ISSET( sds , writefds )) {
            printf("CONEXIÓN %d, ENTER WRITE TO SERVER\n", i);
            for( j = 0 ; j < BUFSIZE ; j++ ) {
                aux_buf[j] = '\0';
            }
            size_t  rbytes  = 0;
            uint8_t *ptr    = buffer_read_ptr(buffer[i][0], &rbytes);
            memcpy(aux_buf,ptr,rbytes);
            if(strlen(aux_buf) > BUFSIZE){
                aux_buf[BUFSIZE] = '\0';
            }

            if( (buffer[i][0]->read + rbytes) < buffer[i][0]->limit ){
                rbytes++;
            }

            buffer_read_adv(buffer[i][0], rbytes);

            if ( write( sds , aux_buf, strlen(aux_buf)) != strlen(aux_buf)) {
                perror("write failed");       
            }
            descriptors_arrays->server_sockets_write[i] = 0;
        }


        sdc = descriptors_arrays->client_sockets_read[i];
        sds = descriptors_arrays->server_sockets_read[i];

        //reading requests from client  
        if (FD_ISSET( sdc , readfds)) {  
            printf("CONEXIÓN %d, ENTER READ FROM CLIENT\n", i);
            for( j = 0 ; j < BUFSIZE ; j++ ) {
                aux_buf[j] = '\0';
            }
            //Handle client conection
            int handleResponse = read( sdc , aux_buf, size_to_write(buffer[i][1]));
            str_size = 0;
            for( j = 0; j < BUFSIZE; j++){
                if(aux_buf[j]!='\0'){
                    str_size++;
                }
                else break;
            }
            if ( str_size < BUFSIZE && aux_buf[str_size] == '\0'){
                str_size++;
            }

            //write into server buffer
            size_t  wbytes  = 0;
            uint8_t *ptr    = buffer_write_ptr(buffer[i][0], &wbytes);

            if( wbytes < str_size ){
                perror("no space in buffer to read");
            }
            memcpy(ptr, aux_buf, str_size);
            buffer_write_adv(buffer[i][0], str_size);
            
            if( handleResponse == 0 ) {
                //Somebody disconnected , get his details and print
                getpeername(sdc , (struct sockaddr*)client_address , (socklen_t*)client_addr_len);
                printf("Host disconnected , ip %s , port %d \n" , inet_ntoa((*client_address).sin_addr) , ntohs((*client_address).sin_port));
                      
                //Close the socket and mark as 0 in list for reuse, or
                printf("CONEXIÓN %d, CIERRA CLIENT\n", i);
                close( sdc );
                close( sds );
                descriptors_arrays->client_sockets_read[i] = 0;
                descriptors_arrays->server_sockets_read[i] = 0;
                buffer_reset(buffer[i][0]);
                buffer_reset(buffer[i][1]);
            }
            else if(handleResponse < 0) {
                perror("read failed");
            }
            else {
                printf("Read something from client\n");
                descriptors_arrays->server_sockets_write[i] = descriptors_arrays->server_sockets_read[i]; 
            }
        }

        //reading responses from server
        if (FD_ISSET( sds , readfds)) {
            printf("CONEXIÓN %d, ENTER READ FROM SERVER\n", i);
            for( j = 0 ; j < BUFSIZE ; j++ ) {
                aux_buf[j] = '\0';
            }
            //Handle server conection
            int handleResponse = read( sds , aux_buf, size_to_write(buffer[i][1]));
            
            str_size = 0;
            for( j = 0; j < BUFSIZE; j++){
                if(aux_buf[j]!='\0'){
                    str_size++;
                }
                else break;
            }
            if ( str_size < BUFSIZE && aux_buf[str_size] == '\0'){
                str_size++;
            }

            //write into server buffer
            size_t  wbytes  = 0;
            uint8_t *ptr    = buffer_write_ptr(buffer[i][1], &wbytes);
            if( wbytes < str_size ){
                perror("no space in buffer to read");
            }
            memcpy(ptr, aux_buf, str_size);
            buffer_write_adv(buffer[i][1], str_size);

            if( handleResponse == 0 ) {
                //Somebody disconnected , get his details and print
                getpeername(sdc , (struct sockaddr*)client_address , (socklen_t*)client_addr_len);
                printf("Server disconnected\n");
                      
                //Close the socket and mark as 0 in list for reuse
                printf("CONEXIÓN %d, CIERRA SERVER\n", i);
                close( sdc );
                close( sds );
                descriptors_arrays->client_sockets_read[i] = 0;
                descriptors_arrays->server_sockets_read[i] = 0;
                buffer_reset(buffer[i][0]);
                buffer_reset(buffer[i][1]);
            }
            else if(handleResponse < 0) {
                perror("read failed");
            }
            else {
                printf("Read something from server\n");
                descriptors_arrays->client_sockets_write[i] = descriptors_arrays->client_sockets_read[i]; 
            }
        }
        
    }    
}
