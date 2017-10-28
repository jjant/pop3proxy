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
#include "proxyUtilities.h"
#include "buffer.h"

pthread_t threadForConnection;
extern struct Metrics  metrics;
extern struct Settings settings;
extern FILE *retrieved_mail;

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
    //settings.pop3_address =;
    //settings.management_address =;
    settings.replacement_message = "Parte reemplazada...";
    settings.censurable = "";
    settings.management_port = 9090;
    settings.pop3_port = 1110;
    settings.origin_port = 110;
    settings.cmd = "";
    settings.version = "0.0.0";
}

void setSocketType( struct sockaddr_in* client_address, int port ) {
    //type of socket created for client
    client_address->sin_family      = AF_INET;
    client_address->sin_addr.s_addr = INADDR_ANY;
    client_address->sin_port        = htons( port );
}

int addDescriptors( struct DescriptorsArrays* dA, int **conf_sockets_array, fd_set* readfds, fd_set* writefds, int n, int m) {
    int sdc, sds, conf_ds, max_sd;
    max_sd = 0;

    for ( int j = 0 ; j < m ; j++){
        conf_ds = (*conf_sockets_array)[j];
        if (conf_ds > 0){
            FD_SET( conf_ds , readfds);
            if(conf_ds > max_sd)
                max_sd = conf_ds;
        }
    }

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

void handleConfConnection(int conf_master_socket, struct sockaddr_in *conf_address, int *conf_addr_len, int **conf_sockets_array, int *conf_sockets_amount, pthread_mutex_t *mtx){
    int     new_conf_socket;
    int     add_fd              = 1;
    char    *message            = "Connecting to POP3 proxy for configuration... \r\n";

    if ((new_conf_socket = accept(conf_master_socket, (struct sockaddr *)conf_address, (socklen_t*)conf_addr_len))<0) {
        perror("---config--- accept failed");
        exit(EXIT_FAILURE);
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
    char                *message                                    = "Connecting through POP3 proxy... \r\n";
    int                 add_fd                                      = 1;
    int                 new_server_socket , new_client_socket, rv;
    struct ThreadArgs   *tArgs;
    struct addrinfo     hints, *servinfo, *p;
 
    pthread_detach(pthread_self());

    tArgs = (struct ThreadArgs *)args;

    if ((new_client_socket = accept(tArgs->master_socket, (struct sockaddr *)tArgs->client_address, (socklen_t*)tArgs->client_addr_len))<0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family     = AF_INET;
    hints.ai_socktype   = SOCK_STREAM;

    //in case of receiving an IP, the creation of this thread is unnecessary. Check later.
    //change localhost and pop3 strings.
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
        tArgs->dA->client_sockets_read[*(tArgs->clients_amount)-1]          = new_client_socket;
        tArgs->dA->server_sockets_read[*(tArgs->clients_amount)-1]          = new_server_socket;
        tArgs->dA->client_sockets_write[*(tArgs->clients_amount)-1]         = 0;
        tArgs->dA->server_sockets_write[*(tArgs->clients_amount)-1]         = 0;
        tArgs->dA->client_commands_to_process[*(tArgs->clients_amount)-1]   = 0;
        tArgs->dA->server_commands_to_process[*(tArgs->clients_amount)-1]   = 0;
    }
    (*(tArgs->info_clients))[*(tArgs->clients_amount)-1].retr_invoked = 0;
    (*(tArgs->info_clients))[*(tArgs->clients_amount)-1].user_name    = NULL;     
    pthread_mutex_unlock(tArgs->mtx);

    printf("New connection: Client socket fd: %d , ip: %s , port: %d\n" , new_client_socket , inet_ntoa(tArgs->client_address->sin_addr) , ntohs(tArgs->client_address->sin_port));
    printf("                Server socket fd: %d , ip: %s , port: %d\n" , new_server_socket , settings.origin_server , settings.origin_port);
    
    //send signal to main thread to let it know that the domain name has been resolved and the connection has been made 
    pthread_kill( tArgs->tId, SIGUSR1);

    //free(tArgs);
    pthread_exit(0);
    //return NULL;
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
    *(tArgs->info_clients)                  = realloc(*(tArgs->info_clients), (*(tArgs->clients_amount)+1)*sizeof(struct InfoClient));
    *(tArgs->buffers)                       = realloc(*(tArgs->buffers), (*(tArgs->clients_amount)+1)*sizeof(struct buffer**));
    
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

void handleIOOperations(struct DescriptorsArrays *descriptors_arrays, int **conf_sockets_array, fd_set *readfds, fd_set *writefds, struct buffer ****b, struct sockaddr_in *client_address, struct sockaddr_in *conf_address, int *client_addr_len, int *conf_addr_len, int clients_amount, int conf_sockets_amount, struct InfoClient** info_clients) {
    int             k, l, i, j, sdc, sds, conf_ds, str_size;
    char            aux_buf[BUFSIZE], aux_conf_buf[BUFSIZE];
    struct buffer   ***buffer                                   = *b;

    //handle config admins
    for (k = 0; k < conf_sockets_amount; k++ ){
        conf_ds = (*conf_sockets_array)[k];
        
        if (FD_ISSET( conf_ds , readfds )) {
            int resp;
            const char *ok_msg = "-OK-\n";
            const char *er_msg = "-ERROR-\n";
            const char *cc_msg = "-Concurrent connections-\n";
            const char *ha_msg = "-Historical accesses-\n";
            const char *tb_msg = "-Transfered Bytes-\n";

            printf("---config--- CONEXIÓN %d, ENTER READ\n", k);
            for( l = 0 ; l < BUFSIZE ; l++ ) {
                aux_conf_buf[l] = '\0';
            }
            int handleResponse = read( conf_ds , aux_conf_buf, BUFSIZE);   
            
            if( handleResponse == 0 ) {
                //Somebody disconnected , get his details and print
                getpeername(conf_ds , (struct sockaddr*)conf_address , (socklen_t*)conf_addr_len);
                printf("---config--- Config Host disconnected , ip %s , port %d \n" , inet_ntoa((*conf_address).sin_addr) , ntohs((*conf_address).sin_port));
                      
                //Close the socket and mark as 0 in list for reuse, or
                printf("---config--- CONEXIÓN %d, CIERRA ADMIN\n", k);
                close( conf_ds );
                (*conf_sockets_array)[k] = 0;
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
                case 0:     if ( write( conf_ds , er_msg, strlen(er_msg) ) != strlen(er_msg) ) {
                                perror("---config--- write failed"); 
                            }
                            break;   
                case 1:     if ( write( conf_ds , ok_msg, strlen(ok_msg) ) != strlen(ok_msg) ) {
                                perror("---config--- write failed"); 
                            }
                            break;
                case 2:     if ( write( conf_ds , cc_msg, strlen(cc_msg) ) != strlen(cc_msg) ) {
                                perror("---config--- write failed"); 
                            }
                            break;
                case 3:     if ( write( conf_ds , ha_msg, strlen(ha_msg) ) != strlen(ha_msg) ) {
                                perror("---config--- write failed"); 
                            }
                            break;
                case 4:     if ( write( conf_ds , tb_msg, strlen(tb_msg) ) != strlen(tb_msg) ) {
                                perror("---config--- write failed"); 
                            }
                            break;
                default:    break;
                }
            }

        }
    }

    //handle pop3 clients
    for (i = 0; i < clients_amount; i++) {
        
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

            //in case of reading an entire command, add space for '\0'
            if( (buffer[i][1]->read + rbytes) < buffer[i][1]->limit ){
                rbytes++;
            }

            buffer_read_adv(buffer[i][1], rbytes);
            if ( write( sdc , aux_buf, strlen(aux_buf)) != strlen(aux_buf)) {
                perror("write failed");       
            }  
            (descriptors_arrays->server_commands_to_process[i])--;
            if( (descriptors_arrays->server_commands_to_process[i]) == 0 ){
                descriptors_arrays->client_sockets_write[i] = 0;
            }
        }
        //writing to server
        if (FD_ISSET( sds , writefds ) ) {
            enum Command { USER = 0, RETR, CAPA, OTHER };
            int  cmd;
            printf("CONEXIÓN %d, ENTER WRITE TO SERVER\n", i);
            for( j = 0 ; j < BUFSIZE ; j++ ) {
                aux_buf[j] = '\0';
            }
            size_t  rbytes  = 0;
            //If server does not support pipelining, sends one command at a time
            //If it does... do later
            //buffer_read_ptr_for_client reads only one command
            //To implement pipelining, do some kind of iteration and keep sending all commands
            uint8_t *ptr    = buffer_read_ptr_for_client(buffer[i][0], &rbytes);
            memcpy(aux_buf,ptr,rbytes);
            if(strlen(aux_buf) > BUFSIZE){
                aux_buf[BUFSIZE] = '\0';
            }

            //Analyse sending command
            cmd = parseClientCommand(aux_buf);
            if( cmd == USER ){
                //set name in info_client struct
                //to see if command is 'USER ___', it needs to escape the first 5 chars
                if (strlen(aux_buf) >= 6){
                    (*info_clients)[i].user_name = malloc(strlen(aux_buf+5+1)*sizeof(char)); //+1 for \0
                    memcpy((*info_clients)[i].user_name, aux_buf+5, strlen(aux_buf+5+1));
                    (*info_clients)[i].retr_invoked = 0;
                    printf("New user: %s\n", (*info_clients)[i].user_name);
                }
                (*info_clients)[i].capa_invoked = 0;
            }
            else if( cmd == RETR ){
                //to see if command is 'RETR _', it needs to escape the first 6 chars (counting \n)
                if (strlen(aux_buf) >= 7){
                    (*info_clients)[i].retr_invoked = 1;
                }
                (*info_clients)[i].capa_invoked = 0;
            }
            else if ( cmd == CAPA ){
                (*info_clients)[i].capa_invoked = 1;
                (*info_clients)[i].retr_invoked = 0;
            }
            else {
                (*info_clients)[i].capa_invoked = 0;
                (*info_clients)[i].retr_invoked = 0;
            }


            buffer_read_adv(buffer[i][0], rbytes);
            if ( write( sds , aux_buf, strlen(aux_buf)) != strlen(aux_buf)) {
                perror("write failed");       
            }
            (descriptors_arrays->client_commands_to_process[i])--;
            if( (descriptors_arrays->client_commands_to_process[i]) == 0 ){
                descriptors_arrays->server_sockets_write[i] = 0;
            }
        }


        sdc = descriptors_arrays->client_sockets_read[i];
        sds = descriptors_arrays->server_sockets_read[i];

        //reading requests from client  
        if ( FD_ISSET(sdc , readfds) ) { 


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
                (descriptors_arrays->client_commands_to_process[i])++;
                descriptors_arrays->server_sockets_write[i] = descriptors_arrays->server_sockets_read[i]; 
            }
        }

        //reading responses from server
        if ( FD_ISSET(sds , readfds) ) {
            printf("CONEXIÓN %d, ENTER READ FROM SERVER\n", i);
            for( j = 0 ; j < BUFSIZE ; j++ ) {
                aux_buf[j] = '\0';
            }
            //Handle server conection
            int handleResponse = read( sds , aux_buf, size_to_write(buffer[i][1]));

            if ( (*info_clients)[i].capa_invoked == 1 ) {
                if ( parseServerForCAPA(aux_buf) == 1 ) {
                    (*info_clients)[i].pipeline_supported = 1;
                }
                else {
                    (*info_clients)[i].pipeline_supported = 0;
                }
            }
            else if ((*info_clients)[i].retr_invoked == 1 ){
                //Transformation needs to be applied
                
                retrieved_mail = fopen("./retrieved_mail.txt", "at");
                fprintf(retrieved_mail,"%s", aux_buf);
                fclose(retrieved_mail);
                printf("\nRETR invoked and response received\n\n");
            }

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

            //write into client buffer
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
                //Here we should fork() or pthread_create() and call the transformation program 
                (descriptors_arrays->server_commands_to_process[i])++;
                descriptors_arrays->client_sockets_write[i] = descriptors_arrays->client_sockets_read[i]; 
            }
        }
        printf("Client commands to process: %d\n", (descriptors_arrays->client_commands_to_process[i]));
    }    
}

int parseClientCommand(char b[]){
    int     idx  = 0;
    char    c;

    c = b[idx];
    switch( toupper(c) ){
        case 'U':   if ( analyzeString( b+1, "SER" ) == 1 ) {
                        return 0;
                    }
                    else 
                        return 3;
        case 'R':   if ( analyzeString( b+1, "ETR" ) == 1 )
                        return 1;
                    else 
                        return 3;
        case 'C':   if ( analyzeString( b+1, "APA" ) == 1 )
                        return 2;
                    else 
                        return 3;
        default:    return 3;
    }
}

int analyzeString( const char buffer[], const char *command) {
    enum StringState { READ = 0, RIGHT, WRONG };
    
    int  bufIndex  = 0; 
    int  comIndex  = 0;
    int  state     = READ;
    while ( state == READ ){
        char uppercaseLetter = toupper( buffer[bufIndex] );
        if ( command[comIndex] == '\0' && ( uppercaseLetter == '\n' || uppercaseLetter == ' ' ) ) {
            state = RIGHT;
        }
        else if ( uppercaseLetter == '\0' || ( uppercaseLetter != command[comIndex] ) ) {
            state = WRONG;
        }
        comIndex++;
        bufIndex++;
    }
    return state;
}

int parseServerForCAPA(char b[]){
    for (int i = 0; b[i] != '.'; i++) {
        if ( b[i] == 'P' ) {
            if ( strncmp( b+i+1, "IPELINING", 9) == 0 ) {
                return 1;
            }
        }
    }
    return 0;
}

int parseConfigCommand(char b[]){
    if ( strncmp( b, "OS ", 3) == 0 ){
        //Set origin server
        settings.origin_server = malloc (strlen(b+3));
        memcpy(settings.origin_server, b+3, strlen(b+3)+1);
        printf("%s\n", settings.origin_server);
        return 1;
    }
    else if ( strncmp( b, "EF ", 3) == 0 ){
        //Set error file
        settings.error_file = malloc (strlen(b+3));
        memcpy(settings.error_file, b+3, strlen(b+3)+1);
        printf("%s\n", settings.error_file);
        return 1;
    }
    else if ( strncmp( b, "PA ", 3) == 0 ){
        //Set POP3 address
        return 1;
    }
    else if ( strncmp( b, "MA ", 3) == 0 ){
        //Set management address
        return 1;
    }
    else if ( strncmp( b, "RM ", 3) == 0 ){
        //Set replacement message
        settings.replacement_message = malloc (strlen(b+3));
        memcpy(settings.replacement_message, b+3, strlen(b+3)+1);
        return 1;
    }
    else if ( strncmp( b, "CT ", 3) == 0 ){
        //Set censurable types
        settings.censurable = malloc (strlen(b+3));
        memcpy(settings.censurable, b+3, strlen(b+3)+1);
        return 1;
    }
    else if ( strncmp( b, "MP ", 3) == 0 ){
        //Set management port
        return 1;
    }
    else if ( strncmp( b, "PP ", 3) == 0 ){
        //Set POP3 port
        return 1;
    }
    else if ( strncmp( b, "OP ", 3) == 0 ){
        //Set origin port
        return 1;
    }
    else if ( strncmp( b, "CD ", 3) == 0 ){
        //Set command
        settings.cmd = malloc (strlen(b+3));
        memcpy(settings.cmd, b+3, strlen(b+3)+1);
        return 1;
    }
    else if ( strncmp( b, "SCC", 3) == 0 ){
        //Set concurrent connections metrics on
        metrics.cc_on = 1;
        return 1;
    }
    else if ( strncmp( b, "RCC", 3) == 0 ){
        //Set concurrent connections metrics on (reset)
        metrics.cc_on = 0;
        return 1;
    }
    else if ( strncmp( b, "GCC", 3) == 0 ){
        //Get concurrent connections metrics
        return 2;
    }
    else if ( strncmp( b, "SHA", 3) == 0 ){
        //Set historical accesses metrics on
        metrics.ha_on = 1;
        return 1;
    }
    else if ( strncmp( b, "RHA", 3) == 0 ){
        //Set historical accesses metrics on (reset)
        metrics.ha_on = 0;
        return 1;
    }
    else if ( strncmp( b, "GHA", 3) == 0 ){
        //Get historical accesses metrics
        return 3;
    }
    else if ( strncmp( b, "STB", 3) == 0 ){
        //Set transfered bytes metrics on
        metrics.tb_on = 1;
        return 1;
    }
    else if ( strncmp( b, "RTB", 3) == 0 ){
        //Set transfered bytes metrics on (reset)
        metrics.tb_on = 0;
        return 1;
    }
    else if ( strncmp( b, "GTB", 3) == 0 ){
        //Get transfered bytes metrics
        return 4;
    }
    else
        return 0;
}