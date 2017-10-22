#ifndef PROXY_UTILITIES_H
#define PROXY_UTILITIES_H

#define TRUE   1
#define FALSE  0
#define PORT 8888
#define CONFPORT 7777
#define PORTSERVER 110
#define BUFSIZE 128
#define MAXCLIENTS 128

struct DescriptorsArrays{
    int *client_sockets_read;
    int *client_sockets_write;
    int *server_sockets_read;
    int *server_sockets_write;
    int *client_commands_to_process;
    int *server_commands_to_process;
};

struct ThreadArgs{
	int 						*clients_amount;
	int 						master_socket; 
	int 						new_client_socket;
	int 						*client_addr_len;
	struct sockaddr_in 			*client_address; 
	struct DescriptorsArrays 	*dA;
	pthread_t 					tId;
	pthread_mutex_t 			*mtx;
	struct buffer				****buffers;
};

/**
 * Configures the type of socket for clients
 */
void setSocketType(struct sockaddr_in*, int);

/**
 * Adds descriptors currently connected to read and writes sets (config sockets too). 
 * It's used in the select function.
 */
int addDescriptors(struct DescriptorsArrays*, int**, fd_set*, fd_set*, int, int);

/**
 * Accepts new config connection.
 */
void handleConfConnection(int, struct sockaddr_in*, int*, int**, int*, pthread_mutex_t*);

/**
 * Accepts new connection and connects another socket to server (calling handleThreadForConnection).
 */
void handleNewConnection(int, struct sockaddr_in*, int*, struct DescriptorsArrays*, pthread_mutex_t* mtx, int*, struct buffer****);

/**
 * Resolves DNS and connects with server, in separated thread.
 * Sets the corresponding file descriptors in descriptors arrays.
 */
void* handleThreadForConnection(void*);

/**
 * Reserves space for buffers and descriptors arrays if needed (on new clients arrivals)
 */
void reserveSpace(struct ThreadArgs*);

/**
 * Handles read and write operations, on files identified in descriptors arrays.
 */
void handleIOOperations(struct DescriptorsArrays*, int**, fd_set*, fd_set*, struct buffer****, struct sockaddr_in* , struct sockaddr_in*, int*, int*, int, int);

#endif