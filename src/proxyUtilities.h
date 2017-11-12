#ifndef PROXY_UTILITIES_H
#define PROXY_UTILITIES_H

#define BUFSIZE 1024
#define MAXCLIENTS 1024

struct DescriptorsArrays{
    int *client_sockets_read;
    int *client_sockets_write;
    int *server_sockets_read;
    int *server_sockets_write;
    int *client_commands_to_process;
    int *server_commands_to_process;
    int *server_sends_closure;
    int *client_needs_closure;
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
	struct InfoClient 			**info_clients;
};

struct InfoClient{
	int 	available_to_write;
	int 	retr_invoked;
	int 	capa_invoked;
	int 	pipeline_supported;
	char 	*user_name;
};

struct Metrics{
	int 				concurrent_connections;
	int 				cc_on;
	int 				historical_accesses;
	int 				ha_on;
	unsigned long int 	transfered_bytes;
	int 				tb_on;
};

struct Settings{
	char *origin_server;		//Def: "127.0.0.1"
	char *error_file;			//Def: "/dev/null"
	char *pop3_address;			//Def: INADDR_ANY
	char *management_address;	//Def: "127.0.0.1"
	char *replacement_message;	//Def: "Parte reemplazada..."
	char *censurable;			//Def: ""
	int  management_port;		//Def: 9090
	int  pop3_port;				//Def: 1110
	int  origin_port;			//Def: 110
	char *cmd;					//Def: ""
	char *version;				//0.0.0
};

void setNullPointers(struct DescriptorsArrays*, int**, struct buffer****, struct InfoClient**);

void setDefaultMetrics();

void setDefaultSettings();

void readArguments(int argc, char *argv[]);

void configureSocket(int*, int, int, int, int, int, int*, struct sockaddr_in*, int*, int, char*);

/**
 * Configures the type of socket for clients
 */
void setSocketType(struct sockaddr_in*, int, char*);

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
void handleNewConnection(int, struct sockaddr_in*, int*, struct DescriptorsArrays*, pthread_mutex_t* mtx, int*, struct buffer****, struct InfoClient**);

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
 * This function calls the specific ones described below.
 */
void handleIOOperations(struct DescriptorsArrays*, int**, fd_set*, fd_set*, struct buffer****, struct sockaddr_in* , struct sockaddr_in*, int*, int*, int, int, struct InfoClient**, int*, int*, int*, int*);

/**
 * Handles managemente IO operations. Uses parsers to analyze requests for configuration.
 */
void handleManagementRequests(int, int**, struct sockaddr_in*, int*, struct sockaddr_in*, int*, int*, int*, int*, int*);

/**
 * Reads from buffer and writes to the client file descriptor.
 */
void writeToClient(int, struct DescriptorsArrays*, struct buffer****);

/**
 * Reads commands from the client file descriptor, analyses them and writes them to the buffer .
 */
void readFromClient(int i, struct DescriptorsArrays*, struct buffer****, struct sockaddr_in*, int*);

/**
 * Reads from buffer and writes to the server file descriptor.
 */
void writeToServer(int, struct DescriptorsArrays*, struct buffer****, struct InfoClient**);

/**
 * When it's created, a child process handles the mail transformation.
 */
void handleChildProcess(int);

/**
 * Reads responses from server, and sends them to the client or to child process accordingly.
 */
void readFromServer(int, struct DescriptorsArrays*, struct buffer****, struct sockaddr_in*, int*, struct InfoClient**);

/**
 * Reads from pipe when child process retrieved transformed mail and writes to the client file descriptor.
 */
void readFromPipe(int, struct DescriptorsArrays*, struct buffer****);

void setEnvironmentVars(int, const char*);

void writeLog(const char*, FILE*);

#endif