#include "rpc.h"
#include <stdlib.h> // malloc, free, exit...
#include <stdio.h> // printf...
#include <string.h> // strcmp, strcpy, strncpy...
#include <unistd.h> // write...
#include <assert.h> // assert
#include <arpa/inet.h> // for converting network addresses and port numbers between their string representations and their numeric binary forms.
#include <netinet/in.h> //  define the sockaddr_in structure, which is used for storing IPv4 addresses and port numbers.
#include <sys/socket.h> // socklen_t, socket(), bind(), listen(), accept(), connect(), and send(), SOCK_STREAM, SOCK_DGRAM
#include <netdb.h> // struct addrinfo, getaddrinfo, gai_strerror, and freeaddrinfo

#define MAX_FUNCTIONS 10
// #define MAX_FUNCTIONS 100
#define MAX_FUNC_NAME 1000
#define BUFFER_SIZE 100000
#define SHUTDOWN_TIME 30 // in seconds


/*   helper functions  */
char* portIntToString(int port);
int create_listening_socket(char *port);
void accept_connection(rpc_server *srv);
char read_type(rpc_server *srv);
void handle_find_request(rpc_server *srv, char* name);
void handle_call_request(rpc_server *srv, char* name);
void handle_payload(rpc_client *cl, rpc_handle *h, rpc_data *payload);
rpc_data *process_request(char data2, int sockfd);


/* function for Server function name and handler */
typedef struct {
    char name[MAX_FUNC_NAME + 1];
    rpc_handler handler;
} func_entry_t;

/* Server functions */
struct rpc_server {
    int sockfd;
    int newsockfd;
    func_entry_t functions[MAX_FUNCTIONS];
    int num_func;
    int num_sockt;
};

/* Initialises server state */
/* RETURNS: rpc_server* on success, NULL on error */
// Called before rpc_register.  It should return a pointer to a struct (that you define) containing 
// server state information on success and NULL on failur
rpc_server *rpc_init_server (int port) { //3000 / 8000
    /* Create server socket and wait for accept */
    int sockfd;
    // convert the port from a number to the corresponding string
    char *ports = portIntToString(port);
    // Create the listening socket
    sockfd = create_listening_socket(ports);
    // Listen on socket - ready to accept connections,
    if (listen(sockfd, 5) < 0) { 
        perror("failed to listen");
        exit(EXIT_FAILURE);
    }
    /* Create an rpc server well and store the information for the upper seat */
    rpc_server* server = malloc(sizeof(rpc_server));
    assert(server != NULL);
    // update socket number
    server->num_sockt = sockfd;
    /* Initialize the rpc_server attributes */
    server->newsockfd = 0;
    server->num_func = 0;
    memset(server->functions, 0, sizeof(server->functions));
    free(ports);
    return server;
}


// helper function to convert int to string
char* portIntToString(int port) {
    // Determine the length of the port string
    int length = snprintf(NULL, 0, "%d", port);
    // Allocate memory for the port string (+1 for null terminator)
    char* portString = (char*)malloc((length + 1) * sizeof(char));
    // Convert the port to a string
    sprintf(portString, "%d", port);
    return portString;
}


// helper function to creates a socket and binds it to the provided port
// This code from week 9 solution: server-1.2.4.c
int create_listening_socket(char* service) {
	int re, s, sockfd;
	struct addrinfo hints, *res;
	// Create address we're going to listen on (with given port number)
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;       // IPv6
	hints.ai_socktype = SOCK_STREAM; // Connection-mode byte streams
	hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
	// node (NULL means any interface), service (port), hints, res
	s = getaddrinfo(NULL, service, &hints, &res);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	// Create socket
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd < 0) {
		perror("failed to socket");
		exit(EXIT_FAILURE);
	}
	// Reuse port if possible
	re = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
		perror("failed to setsockopt");
		exit(EXIT_FAILURE);
	}
	// Bind address to the socket
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("failed to bind");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(res);
	return sockfd;
}


/* Registers a function (mapping from name to handler) */
/* RETURNS: non-negeative number on success, -1 on failure */
// At the server, let the subsystem know what function to call when an incoming request is received.
// Valid character in name:   Only test printable ASCII characters between 32 and 126.
// How the server know the length of name:   Will not test names longer than 1000 bytes.
// Is there a minimum length of name:   Will not test for empty names.
int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    /* Pass in the name of a function (name), and a function (handler), and record the function and name in the server state */
    /* If the function name already exists, the current new function overrides the old one */
    // Will not test for empty names.
    if (srv == NULL || name == NULL || handler == NULL) {
        printf("name or payload is null");
        return -1;
    }
    int name_len = strlen(name);
    for (int i = 0; i < name_len; i++) {
        // Only test printable ASCII characters between 32 and 126.
        if (name[i] < 32 || 126 < name[i]) {
            printf("Invalid character in name\n");
            return -1;
        }
    }
    // Will not test names longer than 1000 bytes.
    if (name_len == 0 || name_len > MAX_FUNC_NAME) {
        printf("Invalid name length\n");
        return -1;
    }
    for (int i=0; i<srv->num_func; i++) {
        if (strcmp(srv->functions[i].name, name) == 0) {
            srv->functions[i].handler = handler;
            // return if FOUND;
            return i;
        }
    }
    // add to functions
    srv->functions[srv->num_func].handler = handler;
    strncpy(srv->functions[srv->num_func].name, name, MAX_FUNC_NAME);
    srv->num_func += 1;
    // return number of functions;
    return srv->num_func;

}


/* Start serving requests */
/*  RETURNS: (Will not usually return) BUT return if srv is NULL/handling SIGINT(not a requirenment) */
// Wait for incoming requests for any of the registered functions, or rpc_find, on the port 
// specified in rpc_init_server of any interface. 
// 1. If call request, call the requested function, send a reply to the caller, and resume waiting for new requests
// 2. If rpc_find, reply to the caller saying whethere the name was found/possibly an error code.
/* If it's an rpc find request, know that at this point the client will send the server */
 /* a function name, server to find out if name exists, and return the result */
/* If the request is an rpc call request, we know that the client will send the server the 
    function name + datal + dataz, and the server will execute the func and return the result */
void rpc_serve_all(rpc_server *srv) {
    if(srv == NULL) {
        perror("no srv");
        exit(EXIT_FAILURE);
    }
    // loop for multi accept
    while(1) {
        // accpet connection between client and server
        accept_connection(srv);
        // loop for multi request
        char name[MAX_FUNC_NAME+1];
        while(1) {
            memset(name, 0, sizeof(name));
            char type = read_type(srv);
            // 'F' find request
            if(type == 'F') {
                handle_find_request(srv, name);
            }
            // 'C' call request
            else if(type == 'C') {
                handle_call_request(srv, name);
            }
            // 'S' shutdown
            else if(type == 'S') {
                break;
            }
        }
    }
}


// Accept a new connection and return the new socket file descriptor. moved from rpc_init_server for multi accept
// This function takes the server state as its argument and returns the new socket file descriptor.
void accept_connection(rpc_server *srv) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_size;

    client_addr_size = sizeof(client_addr);
    srv->newsockfd = accept(srv->num_sockt, (struct sockaddr*)&client_addr, &client_addr_size);
    if(srv->newsockfd < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
}


// Reads the type of the request from the client. 
// This function takes the new socket file descriptor as its argument and returns the type of the request.
char read_type(rpc_server *srv) {
    char type = 'N';
    read(srv->newsockfd, &type, 1);
    return type;
}


// Handle the request if it's a find request. 
// This function takes the new socket file descriptor and the server state as its arguments and returns nothing.
void handle_find_request(rpc_server *srv, char* name) {
    int n = read(srv->newsockfd, name, MAX_FUNC_NAME);
    name[n] = '\0';
    // initialise not founded
    char request = 'n';
    for (int i = 0; i < srv->num_func; i++) {
        if (strcmp(name, srv->functions[i].name) == 0) {
            // function found
            request = 'f';
            break;
        }
    }
    // write request whether function exists or not ('f' or 'n')
    write(srv->newsockfd, &request, 1);
}


// Handle the request if it's a call request. 
// This function takes the new socket file descriptor and the server state as its arguments and returns nothing.
void handle_call_request(rpc_server *srv, char* name) {
    int name_len = 0;
    int64_t data1;
    size_t data2_len_64;
    rpc_data* input_data = malloc(sizeof(rpc_data));
        
    // receive the length of name to determine how long the name byte is
    read(srv->newsockfd, &name_len, sizeof(int));
    read(srv->newsockfd, name, name_len);
    read(srv->newsockfd, &data1, sizeof(int64_t));
    input_data->data1 = (int)be64toh(data1);

    int64_t data2_len;
    read(srv->newsockfd, &data2_len, sizeof(int64_t));
    data2_len_64 = (int)be64toh(data2_len);
    // read data2
    input_data->data2 = malloc(data2_len_64);
    int result_data2 = read(srv->newsockfd, input_data->data2, data2_len_64);
    input_data->data2_len = result_data2;

    // find function and get the result
    for(int i = 0; i < srv->num_func; i++) {                                
        if(strcmp(name, srv->functions[i].name) == 0) {
            //input vars
            rpc_data* result = srv->functions[i].handler(input_data);
            // check if data is valid and return the info to client
            if(result == NULL) {
                // no result null
                write(srv->newsockfd, "n", 1);
            }
            //check if result is valid for task5 bad server test, i means invalid
            else if(result->data2 != NULL &&result->data2_len == 0) {
                // data2 invalid
                write(srv->newsockfd, "e", 1);
            }
            else if (result->data2 == NULL &&result->data2_len != 0) {
                // data2_len invalid
                write(srv->newsockfd, "e", 1);
            }
            // return to client                    
            else if(result->data2_len == 0 && result->data2 == NULL) {
                // both null
                write(srv->newsockfd, "b", 1);
                int64_t data1_64 = htobe64((int64_t)(result->data1));
                write(srv->newsockfd, &data1_64, sizeof(int64_t));
            }
            else if(result->data2_len != 0 && result->data2 != NULL) {
                // yes valid
                write(srv->newsockfd, "y", 1);
                int64_t data1_64 = htobe64((int64_t)(result->data1));
                write(srv->newsockfd, &data1_64, sizeof(int64_t));
                write(srv->newsockfd, result->data2, result->data2_len);
            }
            free(result);
            break;
        }
    }
    free(input_data->data2);
    free(input_data);
}


struct rpc_client {
    /* Add variable(s) for client state */
    int sockfd;
};


struct rpc_handle {
    /* Add variable(s) for handle */
    char function_name[MAX_FUNC_NAME + 1];
    int function_index;
};


/* Client functions */
/* Initialises client state */
/* RETURNS: rpc_client* on success, NULL on error */
// Called before rpc_find/rpc_call. The string addr and integer port are the text-based IP address and 
// numeric port number passed in on the command line. 
// 1.The function should return a non-NULL pointer to a struct (that you define) containing client state 
// information on success and NULL on failure.
rpc_client *rpc_init_client(char *addr, int port) {
    /* Create Client socket and connect to Server */
    int sockfd;
    int s_addrinfo;
    struct addrinfo hints_client, *servinfo, *rp;
    // Create address
    memset(&hints_client, 0, sizeof hints_client);
    hints_client.ai_family = AF_INET6;
    hints_client.ai_socktype = SOCK_STREAM;
    // The getaddrinfo() function combines the functionality provided by the gethostbyname and getservbyname 
    char* ports = portIntToString(port);
    s_addrinfo = getaddrinfo(addr, ports, &hints_client, &servinfo);
    if (s_addrinfo != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s_addrinfo)); 
        return NULL;
    }
    // Connect to first valid result
    for (rp = servinfo; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1)
            continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            // success
            break;
        close(sockfd) ;
    }
    if (rp == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return NULL;
    }
    freeaddrinfo(servinfo) ;
    /* Create an rpc client and store the corresponding information */
    rpc_client* cl = malloc(sizeof(rpc_client));
    assert(cl != NULL);
    // cl->new_sockfd = sockfd;
    cl->sockfd = sockfd;
    free(ports);
    return cl;
}


/* Finds a remote function by name */
/* RETURNS: rpc_handle* on success, NULL on error */
/* rpc_handle* will be freed with a single call to free(3) */
// At the client, tell the subsystem what details are required to place a call.
// The return value is a handle (not handler) for the remote procedure,
// 1. If name is not registered, it should return NULL. 
// 2. If any of the arguments are NULL then NULL should be returned. 
// 3. If the find operation fails, it returns NULL.
// client send name to sever ask if name exists, and read the feedback
// if nothing return NULL, if exists then create a handle, store the info, return handlerpc handle
rpc_handle *rpc_find(rpc_client *cl, char *name) {
    if(cl == NULL || name == NULL) {
        // printf("cl or name null %d %s\n", cl, name);
        return NULL;
    }
    // Declare the type of this query to be a "Find" request by writing "F" to the socket
    // Found
    char type = 'F';
    write(cl->sockfd, &type, 1);
    
    int num_func = write(cl->sockfd, name, strlen(name));
    if(num_func != strlen(name)) {
        // printf("name len %d\n", num_func);
        return NULL;
    }
    char request;
    // Read the server's request. This will be 'f' if the function was found, or 'n' if not found
    num_func = read(cl->sockfd, &request, 1);
    // If the server's request is 'f' (found)
    if(request == 'f') {
        // create a new handle for the function, set its name to the function name, and return it
        rpc_handle* handle = malloc(sizeof(rpc_handler));
        strcpy(handle->function_name, name);
        return handle;
    }
    // If the server's request is not 'f' (found), return NULL
    return NULL;
}


/* Calls remote function using handle */
/* RETURNS: rpc_data* on success, NULL on error */
// It cause the subsystem to run the remote procedure, and return the value.
// 1. If the call fails, it returns NULL. 
// 2. If any of the arguments are NULL, NULL should be returned. 
// 3. If this returns a non-NULL value, then it should dynamically allocate (by malloc) both the rpc_data 
// structure and its data2 field.
// Send function name and data to the server, Receive the request from the server, Return the received request as rpc_data
 rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    // Check if client, handle or payload is null 
    if(cl == NULL || h == NULL || payload == NULL) {
        return NULL;
    }
    // Check the inc between data2 and data2_len. Inspired on ed, there 2 cases
    if(payload->data2_len != 0 && payload->data2 == NULL) {
        return NULL;
    }
    if(payload->data2_len == 0 && payload->data2 != NULL) {
        return NULL;
    }
    // Check if the payload's data1 fits in 64 bits 
    int bits = sizeof(payload->data1);
    if (bits > 64) {
        return NULL;
    } 
    // Send the payload to the server
    handle_payload(cl, h, payload);
    // read from socket
    char data2;
    read(cl->sockfd, &data2, 1);
    // 'n' or 'e' (indicating an error or null response), then return NULL
    if(data2 == 'n' || data2 == 'e') {
        return NULL;
    }
    // Process the server's response and return it
    rpc_data *result = process_request(data2, cl->sockfd);
    return result;
}


// helper function for rpc_call
// Write to socket with converting bits size
void handle_payload(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    // Declear the type of this request (Found and Call)
    char type = 'C';
    write(cl->sockfd, &type, 1);
    // Send the len of name of function to sever
    int name_length = strlen(h->function_name);
    write(cl->sockfd, &name_length, sizeof(int));
    // Send the name of function to sever
    write(cl->sockfd, h->function_name, strlen(h->function_name));
    // convert back from 64 bits
    int64_t data1_64 = htobe64((int64_t)payload->data1);
    int64_t data2_len_64 = htobe64((int64_t)payload->data2_len);
    // send the data1 and 
    write(cl->sockfd, &data1_64, sizeof(int64_t));
    write(cl->sockfd, &data2_len_64, sizeof(int64_t));

    write(cl->sockfd, payload->data2, payload->data2_len);
}


// helper function for rpc_call
// first allocate memory for the rpc_data structure itself. If data2 equals 
// 'y', we additionally allocate memory for data2 and read its value from the socket. 
// If data2 equals 'b', we simply set data2 and data2_len to their respective null values.
/* Allocate memory for result, contains server response if allocation fails, return NULL */
rpc_data *process_request(char data2, int sockfd) {
    // Allocate memory for the rpc_data structure to hold the server's response
    rpc_data *result = malloc(sizeof(rpc_data));
    // If memory allocation fails, return NULL
    if (!result) {
        return NULL;
    }
    // convert to 64 bits
    int64_t data1_64;
    read(sockfd, &data1_64, sizeof(int64_t));
    result->data1 = (int)be64toh(data1_64);
    // yes data is valid, then read it from server
    if(data2 == 'y') {
        result->data2 = malloc(BUFFER_SIZE);
        if (!result->data2) {
            free(result);
            return NULL;
        }
        int result_data2 = read(sockfd, result->data2, BUFFER_SIZE);
        result->data2_len = result_data2;
    }
    // both data is invalid, then set data2 and data2_len to their null values
    else if(data2 == 'b') {
        result->data2 = NULL;
        result->data2_len = 0;
    }
    return result;
}


/* Cleans up client state and closes client */
// Frees the memory allocated for a dynamically allocated rpc_data struct.
void rpc_close_client(rpc_client *cl) {
    if (cl == NULL) {
        return;
    }
    char type = 'S'; // Shoutdown
    write(cl->sockfd, &type, 1);
    close(cl->sockfd);
    free(cl);
}


/* Frees a rpc_data struct */
void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}
