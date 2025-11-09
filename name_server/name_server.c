#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_BUFFER_SIZE 1024
#define NAME_SERVER_PORT 8080

// A struct to pass to our thread function
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} client_info_t;

void* handle_connection(void* client_info_p) {
    client_info_t* client_info = (client_info_t*)client_info_p;
    int sock = client_info->client_socket;
    char* client_ip = inet_ntoa(client_info->client_addr.sin_addr);
    
    char buffer[MAX_BUFFER_SIZE];
    int read_size;

    // Read the message from the client/server
    if ((read_size = recv(sock, buffer, MAX_BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0'; // Null-terminate the string

        // --- Protocol Parsing Logic ---
        char* token = strtok(buffer, ";\n");

        if (token != NULL) {
            if (strcmp(token, "REGISTER_SS") == 0) {
                // This is a Storage Server
                char* ss_ip = strtok(NULL, ";\n");
                char* client_port = strtok(NULL, ";\n");
                char* files = strtok(NULL, ";\n");
                printf("[Name Server] Received Storage Server Registration.\n");
                printf("  -> SS IP: %s\n", ss_ip);
                printf("  -> SS Client Port: %s\n", client_port);
                printf("  -> Files: %s\n\n", files ? files : "None");
                // TODO: Store this information in a proper data structure
            } 
            else if (strcmp(token, "REGISTER_CLIENT") == 0) {
                // This is a User Client
                char* username = strtok(NULL, ";\n");
                printf("[Name Server] Received User Client Registration.\n");
                printf("  -> Username: %s\n", username);
                printf("  -> Client IP: %s\n\n", client_ip);
                 // TODO: Store this information
            } else {
                fprintf(stderr, "Unknown command received.\n");
            }
        }
    }

    if (read_size == 0) {
        printf("Client disconnected.\n");
    } else if (read_size == -1) {
        perror("recv failed");
    }

    close(sock);
    free(client_info);
    pthread_exit(NULL);
}


int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // 1. Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Could not create socket");
        return 1;
    }

    // 2. Prepare the sockaddr_in structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(NAME_SERVER_PORT);

    // 3. Bind
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    printf("[Name Server] Bind successful on port %d.\n", NAME_SERVER_PORT);

    // 4. Listen
    listen(server_sock, 5);
    printf("[Name Server] Waiting for incoming connections...\n");

    // 5. Accept and handle connections
    while ((client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len))) {
        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        pthread_t thread_id;
        client_info_t* client_info = malloc(sizeof(client_info_t));
        client_info->client_socket = client_sock;
        client_info->client_addr = client_addr;

        if (pthread_create(&thread_id, NULL, handle_connection, (void*)client_info) < 0) {
            perror("could not create thread");
            return 1;
        }
        // We don't join the thread because we want to handle multiple connections at once.
        // This is called a "detached" thread model in principle.
    }

    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;
}