#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define NAME_SERVER_IP "127.0.0.1"
#define NAME_SERVER_PORT 8080
#define MAX_USERNAME_LEN 50

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char username[MAX_USERNAME_LEN];
    char message[1024];

    // 1. Get username from user
    printf("Enter your username: ");
    fgets(username, MAX_USERNAME_LEN, stdin);
    username[strcspn(username, "\n")] = 0; // Remove trailing newline

    // 2. Create and connect socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Could not create socket");
        return 1;
    }
    server_addr.sin_addr.s_addr = inet_addr(NAME_SERVER_IP);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NAME_SERVER_PORT);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect to Name Server failed");
        return 1;
    }
    printf("[Client] Connected to Name Server.\n");

    // 3. Format and send registration message
    snprintf(message, sizeof(message), "REGISTER_CLIENT;%s\n", username);
    if (send(sock, message, strlen(message), 0) < 0) {
        perror("Send failed");
        return 1;
    }
    printf("[Client] Registration request sent for user '%s'.\n", username);

    // 4. Close the socket
    close(sock);

    return 0;
}