/*
EPI GIJON - University of Oviedo
DEGREE IN COMPUTER SCIENCE/ENGINEERING
DISTRIBUTED SYSTEMS - 3rd YEAR
COURSE MATERIAL
-------------------------
MECHANISM: P2P
FILE: server-chat.c
DESCRIPTION: The implementation of the client list is very inefficient. The right thing would be to use a hash table or a dictionary, but standard C does not include this type of data, and it goes beyond the objectives of the course implement this type and all its operations, so that a solution is chosen much simpler but inadequate: a fixed-size array containing pointers to client structures, and inefficient searches traversing the entire array.
*/

#include "protocol.h"
#include <string.h>

#define MAX_CLIENTS 200

// Structure that stores the pair (nick, endpoint), that relates a nick
// with its corresponding IP and port
struct Client
{
    char *nick;
    struct sockaddr_in *endpoint;
};
typedef struct Client Client;

Client *clients[MAX_CLIENTS];

// Initialize the array of clients with NULL
void init_clients()
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
        clients[i] = NULL;
}

// Look for an empty place in the array and put a new client pointer there
int insert_client(Client *c)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] == NULL)
        {
            clients[i] = c;
            return 0;
        }
    }
    return -1;
}

// Access element[i] of the array, get the pointer stored there, release
// the memory structures pointed to by that pointer, and set that pointer to NULL
// array element to mark it as free
int delete_client(int i)
{
    Client *c;
    if (clients[i] == NULL)
        return -1;
    c = clients[i];
    clients[i] = NULL;
    free(c->nick);
    free(c->endpoint);
    free(c);
    return 0;
}

// Loop through the array and compare the nickname of the non-null entries with the given nickname
// Returns the index of the first match, or -1 if there is none
int search_client(char *nick)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if ((clients[i] != NULL) && (strcmp(clients[i]->nick, nick) == 0))
            return i;
    }
    return -1;
}

// -------------------------------------------------------------------
// These functions are the ones used by the protocol when a client
// register with the server, or unregister or ask for another
// client

// A new client joins, its data is stored in the list
// Returns 0 if everything goes well, -1 if there is an error, which almost
// always means that the nickname was already registered. Also
// can return -1 if there is no memory for the new client or if
// all positions of the array are occupied, but in these cases
// will not occur normally during practice sessions.
int register_s(char *nick, struct sockaddr_in *endpoint)
{
    Client *c;

    // If the nickname is already registered, return error
    if (search_client(nick) != -1)
        return -1;

    // Make memory for the new client and their information
    // returning error if there is a problem.
    c = malloc(sizeof(Client));
    if (c == NULL)
        return -1;
    c->nick = malloc(strlen(nick));
    if (c->nick == NULL)
    {
        free(c);
        return -1;
    }
    c->endpoint = malloc(sizeof(struct sockaddr_in));
    if (c->endpoint == NULL)
    {
        free(c->nick);
        free(c);
        return -1;
    }

    // Copy the parameters received in the new client
    strcpy(c->nick, nick);
    memcpy(c->endpoint, endpoint, sizeof(struct sockaddr_in));

    // DEBUG: show what we are going to save
    printf("Inserting P2P client %s@%s:%d \n",
           c->nick,
           inet_ntoa(c->endpoint->sin_addr),
           ntohs(c->endpoint->sin_port));

    // And insert the client into the list
    if (insert_client(c) == -1)
    {
        free(c->nick);
        free(c->endpoint);
        free(c);
        return -1;
    }
    // All OK
    return 0;
}

// A client does LEAVE. Its data is removed from the list
// Returns -1 if the nick is not found, 0 otherwise
int unregister_s(char *nick)
{
    int c = search_client(nick);
    if (c == -1)
        return -1;
    return delete_client(c);
}

// A client makes a query to find another(by his nickname)
// If found, the resulting structure is filled with the IP and port
// from the client found, and the function returns 0
// If not found, the function returns -1 and the content of the
// the result structure is not modified
int search(char *nick, struct sockaddr_in *result)
{
    Client *c;
    int i = search_client(nick);
    if (i == -1) // Not found
        return -1;

    // If it has been found, we copy the information to the address
    // indicated by the result variable
    c = clients[i];
    memcpy(result, c->endpoint, sizeof(struct sockaddr_in));
    return 0;
}

// The function that reads the command sent by the client from the network and executes it
void execute_command(int socket)
{
    char type;
    short int nick_len;
    char *nick;
    struct sockaddr_in client;
    socklen_t size = sizeof(struct sockaddr_in);
    int port;
    char response;
    int result;

    // Read the command type
    type = ReadByte(socket);

    // Read the nickname of the sender
    nick_len = ReadShort(socket);
    nick = malloc(nick_len + 1);
    ReadString(socket, nick, nick_len);
    nick[nick_len] = 0;

    // Depending on the type read, the action proceeds differently
    switch (type)
    {
    case JOIN_CMD:
        // Get the IP from which the client comes
        getpeername(socket, (struct sockaddr *)&client, &size);
        // DEBUG: we print that information
        printf("JOIN received from %s:%d \n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        // Get the port of the p2p socket, which should transmit it to us
        // now as part of the protocol, in the form of a short
        port = ReadShort(socket);

        // We save it in the previous client structure, overwriting
        // the port that came there, which was not the one of the p2p socket, but the
        // of the socket you used to connect to the server. The IP instead
        // it must be the same, so we leave it
        client.sin_port = ntohs(port);

        // Try to register the nick received
        result = register_s(nick, &client);
        if (result == -1)
        {
            // The nickname was already registered, or other problems
            response = JOIN_ERR;
        }
        else
        {
            response = CMD_OK; // OKAY
        }
        SendByte(socket, response);
        break;
    case LEAVE_CMD:
        // Try to unregister the nick received
        result = unregister_s(nick);
        if (result == -1)
        {
            // The nickname was not registered
            response = LEAVE_ERR;
        }
        else
        {
            response = CMD_OK; // OKAY
        }
        SendByte(socket, response);
        break;
    case QUERY_CMD:
        // We print debugging message
        printf("Nickname data requested | %s  |\n", nick);
        // Find the nick
        result = search(nick, &client);
        if (result == -1)
        {
            // The nickname was not registered
            response = QUERY_ERR;
            SendByte(socket, response);
        }
        else
        {
            response = CMD_OK;
            SendByte(socket, response);
            SendEndpoint(socket, &client);
        }
        break;
    default:
        // Syntax error
        response = CMD_ERR;
        SendByte(socket, response);
    }
    // Free the memory occupied by the nick string that is no longer needed
    //(when registering the nick, the function in charge of it has made a copy
    // of this string)
    free(nick);
}

// Main program
int main(int argc, char *argv[])
{
    int port = 0;
    int listening_socket, data_socket;

    // Check the correct use of the command line
    if (argc < 2)
    {
        printf("Usage: %s  port\n", argv[0]);
        exit(0);
    }
    port = atoi(argv[1]);

    listening_socket = ListeningSocket(port);

    // Client service loop
    while (1)
    {
        // We await the arrival of new connections
        printf("Waiting for clients ...\n");
        data_socket = Accept(listening_socket);
        printf("New client\n");
        execute_command(data_socket);
        close(data_socket);
    }
}