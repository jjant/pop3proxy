#ifndef PROXY_UTILITIES_H
#define PROXY_UTILITIES_H

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
	struct InfoClient 			**info_clients;
};

struct InfoClient{
	int 	retr_invoked;
	int 	capa_invoked;
	int 	pipeline_supported;
	char 	*user_name;
};

struct Metrics{
	int concurrent_connections;
	int cc_on;
	int historical_accesses;
	int ha_on;
	int transfered_bytes;
	int tb_on;
};

struct Settings{
	char *origin_server;		//Def: "127.0.0.1"
	char *error_file;			//Def: "/dev/null"
	char *pop3_address;			//Def: todas las interfaces
	char *management_address;	//Def: "loopback"
	char *replacement_message;	//Def: "Parte reemplazada..."
	char *censurable;			//Def: ""
	int  management_port;		//Def: 9090
	int  pop3_port;				//Def: 1110
	int  origin_port;			//Def: 110
	char *cmd;					//Def: ""
	char *version;				//0.0.0
};


void setDefaultMetrics();

void setDefaultSettings();

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
 */
void handleIOOperations(struct DescriptorsArrays*, int**, fd_set*, fd_set*, struct buffer****, struct sockaddr_in* , struct sockaddr_in*, int*, int*, int, int, struct InfoClient**);

/**
 * Analyse command to see if client wrote USER, RETR, or CAPA
 */
int parseClientCommand(char b[]);
int analyzeString( const char b[], const char*);

/**
 * Search for PIPELINE capability
 */
int parseServerForCAPA(char b[]);

/**
 * Parse command of configuration and apply it
 */
int parseConfigCommand(char b[]);
#endif