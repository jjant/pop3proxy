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

struct ThreadArgs{
	int master_socket; 
	int new_client_socket;
	struct sockaddr_in* addressClient; 
	int* addrClientlen; 
	struct DescriptorsArrays* dA;
	fd_set* readfds;
	fd_set* writefds;
	pthread_t tId;
	pthread_mutex_t* mtx;
};

/**
 * Initializes descriptors arrays in 0, and buffers in "\0"
 */
void initialiseArrays( struct DescriptorsArrays*, char[MAXCLIENTS][BUFSIZE] );

/**
 * Configures the type of socket for clients
 */
void setSocketType( struct sockaddr_in* );

/**
 * Adds descriptors currently connected to read and writes sets. It's used in the select function
 */
int addDescriptors( struct DescriptorsArrays*, fd_set*, fd_set*);

/**
 * Accepts new connection and connects another socket to server (calling handleThreadForConnection). 
 */
void handleNewConnection( int , struct sockaddr_in*, int* , struct DescriptorsArrays* , fd_set*, fd_set*, pthread_mutex_t* mtx);

/**
 * Resolves DNS and connects with server, in separated thread.
 * Sets the corresponding file descriptors in descriptors arrays.
 */
void* handleThreadForConnection( void* args );

/**
 * Handles read and write operations, on files identified in descriptors arrays.
 */
void handleIOOperations(struct DescriptorsArrays*, fd_set*, fd_set*, char [MAXCLIENTS][BUFSIZE], struct sockaddr_in* , int*);

#endif