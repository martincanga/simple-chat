/*
EPI GIJON - University of Oviedo
DEGREE IN COMPUTER SCIENCE/ENGINEERING
DISTRIBUTED SYSTEMS - 3rd YEAR
COURSE MATERIAL
-------------------------
MECHANISM: P2P
FILE: simple-client-chat.c
DESCRIPTION: This client implements communication between peers through the UDP protocol. It uses select to multiplex the reads between the keyboard and the p2p socket.
*/

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#define MAX_LINE_SIZE 500
#define MAX_MSG_SIZE 500
#define MAX_NICK_SIZE 10

char *nick; // To send it in each message

// These functions are implemented after main()
void read_and_process_keyboard(int udp_socket);
void receive_and_show_message(int udp_socket);
int get_max(int a, int b);

int main(int argc, char *argv[])
{
    int udp_socket;
    int port; // For the socket between peers
    struct sockaddr_in udp_addr;
    int keyboard = 0; // Descriptor of the standard input

    // For the select
    fd_set listen;
    int max;
    int result;

    // Checking arguments
    if (argc != 3)
    {
        printf("Usage: %s <nick> <port>\n", argv[0]);
        exit(0);
    }

    nick = argv[1];
    // Truncate nick if necessary
    if (strlen(nick) >= MAX_NICK_SIZE - 1)
        nick[MAX_NICK_SIZE] = 0;

    port = atoi(argv[2]);

    // Socket initialization
    // We create the UDP socket in the port that is passed to it by command line
    udp_socket = socket(PF_INET, SOCK_DGRAM, 0);
    if (udp_socket == -1)
    {
        perror("When creating the UDP socket");
        exit(1);
    }
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_addr.sin_port = htons(port);
    result = bind(udp_socket, (struct sockaddr *)&udp_addr, sizeof(struct sockaddr_in));
    if (result == -1)
    {
        perror("When doing bind");
        exit(1);
    }

    printf("Use the /CHAT <ip> <port> command to indicate the destination of your messages\n");
    printf("The text you write later will be sent to that contact\n\n");
    printf("At any time you can put /CHAT <ip> <port> again\n");
    printf("to switch to a new contact.\n\n");

    // And we enter the waiting loop
    printf("%s>", nick);
    while (1)
    {
        // Print a prompt to invite to write
        fflush(stdout); // To get it out now, since it doesn't end with \n

        // TODO
        // Select initialization
        FD_ZERO(&listen);
        FD_SET(udp_socket, &listen);
        FD_SET(keyboard, &listen);

        // TODO
        // Wait in select for the user to write something, or for something to arrive
        // over the UDP socket
        max = get_max(udp_socket, keyboard) + 1;
        select(max, &listen, NULL, NULL, NULL);

        // When exiting, something has happened
        if (FD_ISSET(udp_socket, &listen))
        {
            receive_and_show_message(udp_socket);
        }

        if (FD_ISSET(keyboard, &listen))
        {
            read_and_process_keyboard(udp_socket);
        }
    }
}

void receive_and_show_message(int udp_socket)
{
    char buff[MAX_MSG_SIZE];
    int received;

    received = recvfrom(udp_socket, buff, MAX_MSG_SIZE, 0, NULL, NULL);
    if (received == -1)
        return; // We silently ignore errors
    buff[received] = 0;
    printf("\n\t\t%s\n", buff);
    printf("\n%s> ", nick);
}

void read_and_process_keyboard(int udp_socket)
{
    char line[MAX_LINE_SIZE];
    char cmd[MAX_LINE_SIZE];
    static char destination_ip[25] = "Unassigned";
    static int destination_port = 0;
    static struct sockaddr_in target_dir;
    char *message_to_send;
    int i;

    // Read the line
    fgets(line, MAX_LINE_SIZE - MAX_NICK_SIZE, stdin);
    // Remove the carriage return or spaces at the end
    for (i = strlen(line) - 1; i >= 0; i--)
    {
        if (isspace(line[i]))
            line[i] = 0;
        else
            break;
    }
    // Get a new prompt
    printf("%s>", nick);

    // See if it's a command
    if (line[0] == '/')
    {
        // If begins with /, it is a command
        // The easiest thing is to read its contents with sscanf
        sscanf(line, "%s", cmd);

        // TODO
        // Depending on the value found in cmd, we perform the appropriate action
        if(strcmp(cmd, "/CHAT") == 0) {
            sscanf(line, "%s %s %d", cmd, destination_ip, &destination_port);
            target_dir.sin_family = AF_INET;
            target_dir.sin_addr.s_addr = inet_addr(destination_ip);
            target_dir.sin_port = htons(destination_port);
        } else if(strcmp(cmd, "/EXIT") == 0) {
            exit(0);
        } else {
            printf("Unknown command\n");
        }
    }

    else // If the line does not start with /, it is a message to send to the peer
    {
        // You have to send it to the ip and ports previously assigned,
        // but first we check if they were actually assigned
        // before these values
        if (strcmp(destination_ip, "Unassigned") == 0)
        {
            printf("ERROR: before sending message you must use the command\n");
            printf("/CHAT <ip> <port>\n");
            return;
        }
        // If everything goes well, we send the message,
        // preceded by the user's nick

        // TODO
        // Create a buffer with the message to send and send it
        message_to_send = malloc(strlen(nick) + strlen(line) + 3);
        sprintf(message_to_send, "%s> %s", nick, line);

        sendto(udp_socket, message_to_send, strlen(message_to_send), 0, (struct sockaddr *) &target_dir, sizeof(target_dir));      
        
        free(message_to_send);
    }
}

int get_max(int a, int b)
{
    if (a > b)
    {
        return a;
    }
    else
    {
        return b;
    }
}