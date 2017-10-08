/**
    Handle multiple socket connections with select and fd_set on Linux
*/
#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/select.h>
#include <stdbool.h>
  
#define TRUE   1
#define FALSE  0
#define PORT 8888
#define PORTSERVER 110
#define BUFSIZE 128
#define MAXCLIENTS 30

int main(int argc , char *argv[])
{
    int opt = TRUE;
    int master_socket , new_server_socket , addrClientlen , addrServerlen , new_client_socket , activity, i , j ,valread , sdc , sds;
    int client_sockets_read[MAXCLIENTS] , client_sockets_write[MAXCLIENTS] , server_sockets_read[MAXCLIENTS] , server_sockets_write[MAXCLIENTS];
    int max_sd;
    struct sockaddr_in addressClient, addressServer;
      
    char buffer[MAXCLIENTS][BUFSIZE];
      
    //set of socket descriptors
    fd_set readfds, writefds;
      
    //initial message
    char *message = "Connection through proxy... \r\n";
  
    //initialise all client_socket[] to 0 so not checked, and buffers in \0
    for (i = 0; i < MAXCLIENTS; i++) 
    {
        client_sockets_read[i] = 0;
        client_sockets_write[i] = 0;
        server_sockets_read[i] = 0;
        server_sockets_write[i] = 0;

        for( j = 0 ; j < BUFSIZE ; j++ ){
            buffer[i][j] = '\0';
        }
    }
      
    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
    {
        perror("master socket failed");
        exit(EXIT_FAILURE);
    }
    

    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
  
    //type of socket created for client
    addressClient.sin_family = AF_INET;
    addressClient.sin_addr.s_addr = INADDR_ANY;
    addressClient.sin_port = htons( PORT );
    //type of socket created for server. There is only one to test with localhost 110 dovecot
    memset(&addressServer, 0, sizeof(addressServer)); // Zero out structure
    addressServer.sin_family = AF_INET;          // IPv4 address family
    // Convert address
    int rtnVal = inet_pton(AF_INET, "127.0.0.1", &addressServer.sin_addr.s_addr);
    if (rtnVal <= 0){
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }
    addressServer.sin_port = htons(PORTSERVER);    // Server port


    //connect(new_server_socket, (struct sockaddr *)&addressServer, (socklen_t) addrServerlen);

      
    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&addressClient, sizeof(addressClient))<0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Listener on port %d \n", PORT);
     
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
      
    //accept the incoming connection
    addrClientlen = sizeof(addressClient);
    addrServerlen = sizeof(addressServer);
    puts("Waiting for connections ...");
     
    while(TRUE) 
    {
        //clear the socket sets
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
  
        //add master socket to sets
        FD_SET(master_socket, &readfds);
        FD_SET(master_socket, &writefds);
        max_sd = master_socket;
         
        //add child sockets to set
        for ( i = 0 ; i < MAXCLIENTS ; i++){
            //if valid socket descriptor then add to read list
            //highest file descriptor number, need it for the select function
            sdc = client_sockets_read[i];
            if(sdc > 0)
                FD_SET( sdc , &readfds);
            if(sdc > max_sd)
                max_sd = sdc;
            sds = server_sockets_read[i];
            if(sds > 0)
                FD_SET( sds , &readfds);
            if(sds > max_sd)
                max_sd = sds;

            
            sdc = client_sockets_write[i];             
            if(sdc > 0)
                FD_SET( sdc , &writefds);             
            if(sdc > max_sd)
                max_sd = sdc;
            sds = server_sockets_write[i];             
            if(sds > 0)
                FD_SET( sds , &writefds);             
            if(sds > max_sd)
                max_sd = sds;

            if(client_sockets_read[i] != 0){
                printf("i: %d,   client socket: %d,   server socket: %d\n", i , client_sockets_read[i], server_sockets_read[i]);
            }
        }
  
        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , &writefds , NULL , NULL);
    
        if ((activity < 0) && (errno!=EINTR)) 
        {
            printf("select error");
        }
          
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) 
        {
            if ((new_client_socket = accept(master_socket, (struct sockaddr *)&addressClient, (socklen_t*)&addrClientlen))<0)
            {
                perror("accept failed");
                exit(EXIT_FAILURE);
            }

            //create a server socket
            if( (new_server_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
            {
                perror("server socket failed");
                exit(EXIT_FAILURE);
            }

            if ((connect(new_server_socket, (struct sockaddr *)&addressServer, (socklen_t) addrServerlen) )<0)
            {
                perror("connect failed");
            }
          
            //inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_client_socket , inet_ntoa(addressClient.sin_addr) , ntohs(addressClient.sin_port));
        
            //send new connection greeting message
            if( write(new_client_socket, message, strlen(message)) != strlen(message) ) 
            {
                perror("write greeting message failed");
            }
              
            puts("Welcome message sent successfully");
              
            //add new socket to array of sockets
            for (i = 0; i < MAXCLIENTS; i++) 
            {
                //if position is empty
                if( client_sockets_read[i] == 0 )
                {
                    client_sockets_read[i] = new_client_socket;
                    server_sockets_read[i] = new_server_socket;
                    printf("Adding to list of  clients sockets as %d\n" , i);
                    break;
                }
                
            }
            printf("Server socket: %d\n\n", new_server_socket );
        }
          
        //else its some IO operation on some other socket :)
        for (i = 0; i < MAXCLIENTS; i++) 
        {
            sdc = client_sockets_read[i];
            sds = server_sockets_read[i];
              
            if (FD_ISSET( sdc , &readfds)) 
            {
                //Handle client conection
            	int handleResponse = read( sdc , buffer[i], 1024);
                //printf("Size of buffer: %d\n", (int)sizeof(buffer[i]));

            	if( handleResponse == 0 ){
            		//Somebody disconnected , get his details and print
                    getpeername(sdc , (struct sockaddr*)&addressClient , (socklen_t*)&addrClientlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(addressClient.sin_addr) , ntohs(addressClient.sin_port));
                      
                    //Close the socket and mark as 0 in list for reuse
                    close( sdc );
                    client_sockets_read[i] = 0;
                    close( sds );
                    server_sockets_read[i] = 0;
            	}
                else if(handleResponse > 0){
                    printf("Reading something from client:\n");
                    /*memcpy( bufferW , buffer , sizeof(buffer)+1);
                    bufferW[sizeof(buffer)] = '\0';*/
                    printf("%s\n", buffer[i] );
                    server_sockets_write[i] = server_sockets_read[i]; 
                }
            }
            if (FD_ISSET( sds , &readfds)) 
            {
                //Handle server conection
                int handleResponse = read( sds , buffer[i], 1024);
                //printf("Size of buffer: %d\n", (int)sizeof(buffer[i]));
                if( handleResponse == 0 ){
                    //Somebody disconnected , get his details and print
                    getpeername(sdc , (struct sockaddr*)&addressClient , (socklen_t*)&addrClientlen);
                    printf("Server disconnected\n");

                    //And now the client should disconnect and exit...
                      
                    //Close the socket and mark as 0 in list for reuse
                    close( sdc );
                    client_sockets_read[i] = 0;
                    close( sds );
                    server_sockets_read[i] = 0;
                }
                if(handleResponse > 0){
                    printf("Reading something from server:\n");
                    /*memcpy( bufferW , buffer , sizeof(buffer)+1 );
                    bufferW[sizeof(buffer)] = '\0';*/
                    printf("%s\n", buffer[i] );
                    client_sockets_write[i] = client_sockets_read[i]; 
                }
            }

            sdc = client_sockets_write[i];
            sds = server_sockets_write[i];
            if (FD_ISSET( sdc , &writefds )) {
                if ( write( sdc , buffer[i], strlen(buffer[i])) != strlen(buffer[i])){
                    perror("write failed");       
                }
                for( j = 0 ; j < BUFSIZE ; j++ ){
                    buffer[i][j] = '\0';
                }
                client_sockets_write[i] = 0;

            }
            if (FD_ISSET( sds , &writefds )) {
                if ( write( sds , buffer[i], strlen(buffer[i])) != strlen(buffer[i])){
                    perror("write failed");       
                }
                for( j = 0 ; j < BUFSIZE ; j++ ){
                    buffer[i][j] = '\0';
                }
                server_sockets_write[i] = 0;
            }
        }

    }
      
    return 0;
}