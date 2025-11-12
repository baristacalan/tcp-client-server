#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#define BUFFER_SIZE 4096
#define SERVER_IP "127.0.0.1"
#define PORT 4040


typedef struct Socket{

    int sockfd; //Socket file descriptor.
    struct sockaddr_in sock_info;

}Socket;


typedef struct Client {

    struct Socket socket;
    char ip_v4_address[16];
    uint16_t port;
    char username[21];

}Client;


int init_socket(struct Socket* sck) {

    if((sck->sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed to initialize!");
        return -1;
    }
    
    memset(&sck->sock_info, 0, sizeof(sck->sock_info));

    sck->sock_info.sin_family = AF_INET;

    printf("Socket initialized!\n");

    return 0;
}



int connect_socket(struct Socket* sck, const char* ip_address, uint16_t port) {
    
    sck->sock_info.sin_port = htons(port);
    inet_pton(AF_INET, ip_address, &sck->sock_info.sin_addr);
    
    if(connect(sck->sockfd, (struct sockaddr*) &sck->sock_info, sizeof(sck->sock_info)) < 0) {
        perror("Socket failed to connect!");
        exit(EXIT_FAILURE);
    }
    
    printf("Socket connected successfully!\n");
    return 0;
}

void init_client(struct Client* client, const char* ip_address, uint16_t port, const char* username) {

    strncpy(client->username, username, sizeof(client->username) - 1);
    strncpy(client->ip_v4_address, ip_address, sizeof(client->ip_v4_address));
    client->username[sizeof(client->username) - 1] = '\0';


    client->port = port;

    init_socket(&client->socket);
    connect_socket(&client->socket, client->ip_v4_address, client->port);
}

int close_socket(struct Socket* sck) {

    if(close(sck->sockfd) < 0) {
        perror("Socket failed to be closed!");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void disconnect_client(struct Client* client) {

    if(client == NULL) {
        perror("Client not initialized!");
        exit(EXIT_FAILURE);
    }

    Socket* sck = &client->socket;

    close_socket(sck);
    client = NULL;
}


int read_message(struct Client* client, char buffer[], size_t buffer_size) {
    if(client == NULL) { perror("Client not initialized!"); return -1; }

    Socket* sck = &client->socket;

    int bytes_read = recv(sck->sockfd, buffer, buffer_size - 1, 0);

    if(bytes_read < 0){ perror("Error while reading messages"); exit(EXIT_FAILURE); }
    if(bytes_read == 0) return 0;

    buffer[bytes_read] = '\0';
    return bytes_read;
}


int write_message(struct Client* client, const char buffer[]) {
    if(client == NULL) { perror("Client not initialized!"); exit(EXIT_FAILURE); }

    int len = strlen(buffer);
    if(len == 0) return 0;

    char temp[BUFFER_SIZE] = {0};
    
    int username_len = (int)strlen(client->username);
    
    memcpy(temp, client->username, (size_t)username_len);
    
    temp[username_len++] = ':';
    temp[username_len++] = ' ';
    int space = (int)sizeof(temp) - username_len - 1; //Remaining space
    
    if (len > space) len = space;
    memcpy(temp + username_len, buffer, (size_t)len);
    
    int total_len = username_len + len;
    temp[total_len] = '\0';

    Socket* sck = &client->socket;

    int bytes_sent = send(sck->sockfd, temp, total_len, 0);

    if(bytes_sent < 0) { perror("Error while sending message"); exit(EXIT_FAILURE); }
    return bytes_sent;
}


int main(int argc, char* argv[]) {

    Client client = {0};

    char buffer[BUFFER_SIZE] = {0};

    init_client(&client, SERVER_IP, PORT, "Baris");

    // printf("Client Socket File Descriptor: %d\n", client.socket.sockfd);
    // printf("Client IPv4 Address: %s\n", client.ip_v4_address);
    // printf("Client Port: %d\n", client.port);
    // printf("Client User Name: %s\n", client.username);

    struct pollfd pfd[2];

    //Poll file descriptor for read.
    //Polls the socket to return any of the following events:
    pfd[0].fd = client.socket.sockfd;
    pfd[0].events = POLLIN | POLLHUP | POLLERR;

    //Poll file descriptor for write.
    //Polls the stdin to return the following event:
    pfd[1].fd = STDIN_FILENO;
    pfd[1].events = POLLIN;

    while(1) {

        printf("> "); fflush(stdout);
        int ready = poll(pfd, 2, -1);
        
        if(ready < 0) {

            perror("poll"); 
            exit(EXIT_FAILURE);
        }

        if(pfd[0].revents & (POLLERR | POLLHUP)) {
            printf("Server closed!\n");
            break;
        }
        //Reads message if there are any pending.
        if(pfd[0].revents & POLLIN) {
            int bytes_read = read_message(&client, buffer, BUFFER_SIZE);
            
            if(bytes_read == 0){
                printf("Server closed!\n"); 
                break;
            }

            printf("\n%s\n", buffer);
        }

        //For write
        if(pfd[1].revents & POLLIN) {
            //printf("> "); fflush(stdout);
            if(!fgets(buffer, sizeof(buffer), stdin)) break;

            if(strncmp(buffer, "/exit", 5) == 0) break;

            int bytes_write = write_message(&client, buffer);
        }

    }

    disconnect_client(&client);

    return 0;
}