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
#include "handleClient.h"

int main(int argc , char *argv[])
{
    int opt = TRUE;
    int master_socket , new_client_socket , new_server_socket , addrClientlen , addrServerlen ;
    int activity ,valread , sdc , sds , max_sd , aux_max_sd , i , j;
    struct sockaddr_in addressClient, addressServer;
    struct DescriptorsArrays descriptorsArrays;
    char buffer[MAXCLIENTS][BUFSIZE];
    //set of socket descriptors
    fd_set readfds, writefds; 
  
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
    setSocketsType( &addressClient, &addressServer );
    addrClientlen = sizeof(addressClient);
    addrServerlen = sizeof(addressServer);
      
    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&addressClient, sizeof(addressClient))<0){
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
         
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, MAXCLIENTS) < 0){
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Listening on port %d, waiting for connections...\n\n", PORT);

     
    while(TRUE){
        //clear the socket sets
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
  
        //add master socket to sets
        FD_SET(master_socket, &readfds);
        FD_SET(master_socket, &writefds);
        max_sd = master_socket;

        //adds child sockets to set         
        //returns the highest file descriptor number, needed for the select function
        aux_max_sd = addDescriptors( &descriptorsArrays, &readfds, &writefds);
        if( aux_max_sd > max_sd )
            max_sd = aux_max_sd;
  
        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , &writefds , NULL , NULL);
    
        if ((activity < 0) && (errno!=EINTR)){
            printf("select error");
        }
          
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)){
            handleNewConnection(master_socket, &addressClient, &addressServer, &addrClientlen , &descriptorsArrays);   
        }
          
        //else it's some IO operation on some other socket
        for (i = 0; i < MAXCLIENTS; i++){
            sdc = descriptorsArrays.client_sockets_read[i];
            sds = descriptorsArrays.server_sockets_read[i];
              
            //reading requests from client  
            if (FD_ISSET( sdc , &readfds)){
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
                    getpeername(sdc , (struct sockaddr*)&addressClient , (socklen_t*)&addrClientlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(addressClient.sin_addr) , ntohs(addressClient.sin_port));
                      
                    //Close the socket and mark as 0 in list for reuse
                    close( sdc );
                    close( sds );
                    descriptorsArrays.client_sockets_read[i] = 0;
                    descriptorsArrays.server_sockets_read[i] = 0;
            	}
                else if(handleResponse < 0){
                    perror("read failed");
                }
                else{
                    printf("Reading something from client:\n");
                    printf("%s\n", buffer[i] );
                    descriptorsArrays.server_sockets_write[i] = descriptorsArrays.server_sockets_read[i]; 
                }
            }
            //reading responses from server
            if (FD_ISSET( sds , &readfds)){
                //resets buffer
                for( j = 0 ; j < BUFSIZE ; j++ ){
                    buffer[i][j] = '\0';
                }
                //Handle server conection
                //Here we should call the parser function and analyse the responses
                int handleResponse = read( sds , buffer[i], BUFSIZE);
                if( handleResponse == 0 ){
                    //Somebody disconnected , get his details and print
                    getpeername(sdc , (struct sockaddr*)&addressClient , (socklen_t*)&addrClientlen);
                    printf("Server disconnected\n");
                      
                    //Close the socket and mark as 0 in list for reuse
                    close( sdc );
                    close( sds );
                    descriptorsArrays.client_sockets_read[i] = 0;
                    descriptorsArrays.server_sockets_read[i] = 0;
                }
                else if(handleResponse < 0){
                    perror("read failed");
                }
                else{
                    printf("Reading something from server:\n");
                    printf("%s\n", buffer[i] );
                    descriptorsArrays.client_sockets_write[i] = descriptorsArrays.client_sockets_read[i]; 
                }
            }

            sdc = descriptorsArrays.client_sockets_write[i];
            sds = descriptorsArrays.server_sockets_write[i];

            //writing to client
            if (FD_ISSET( sdc , &writefds )){
                if ( write( sdc , buffer[i], strlen(buffer[i])) != strlen(buffer[i])){
                    perror("write failed");       
                }
                for( j = 0 ; j < BUFSIZE ; j++ ){
                    buffer[i][j] = '\0';
                }
                descriptorsArrays.client_sockets_write[i] = 0;

            }
            //writing to server
            if (FD_ISSET( sds , &writefds )){
                if ( write( sds , buffer[i], strlen(buffer[i])) != strlen(buffer[i])){
                    perror("write failed");       
                }
                for( j = 0 ; j < BUFSIZE ; j++ ){
                    buffer[i][j] = '\0';
                }
                descriptorsArrays.server_sockets_write[i] = 0;
            }
        }

    }
      
    return 0;
}
