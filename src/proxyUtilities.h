#ifndef PROXY_UTILITIES
#define PROXY_UTILITIES

#define TRUE   1
#define FALSE  0
#define PORT 8888
#define PORTSERVER 110
#define BUFSIZE 128
#define MAXCLIENTS 30

struct DescriptorsArrays{
    int client_sockets_read[MAXCLIENTS];
    int client_sockets_write[MAXCLIENTS];
    int server_sockets_read[MAXCLIENTS];
    int server_sockets_write[MAXCLIENTS];
};

/**
 * Initializes descriptors arrays in 0, and buffers in "\0"
 */
void initialiseArrays( struct DescriptorsArrays*, char[MAXCLIENTS][BUFSIZE] );

/**
 * Configures the types of sockets for clients and servers
 */
void setSocketsType( struct sockaddr_in* , struct sockaddr_in*);

/**
 * Adds descriptors currently connected to read and writes sets. It's used in the select function
 */
int addDescriptors( struct DescriptorsArrays*, fd_set*, fd_set*);

/**
 * Accepts new connection and connects another socket to server. Sets the corresponding file descriptors in descriptors arrays
 */
void handleNewConnection( int , struct sockaddr_in*, struct sockaddr_in*, int* , struct DescriptorsArrays* );

#endif