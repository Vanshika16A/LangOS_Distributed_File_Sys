#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define NAME_SERVER_IP "127.0.0.1"
#define NAME_SERVER_PORT 8080

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char message[1024];

    // Details for this Storage Server
    const char* SS_IP = "127.0.0.1";
    const int SS_CLIENT_PORT = 9001;
    const char* FILES = "project_plan.txt,notes.txt";

    // 1. Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Could not create socket");
        return 1;
    }

    server_addr.sin_addr.s_addr = inet_addr(NAME_SERVER_IP);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NAME_SERVER_PORT);

    // 2. Connect to Name Server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect to Name Server failed");
        return 1;
    }

    printf("[Storage Server] Connected to Name Server.\n");

    // 3. Format registration message
    snprintf(message, sizeof(message), "REGISTER_SS;%s;%d;%s\n", SS_IP, SS_CLIENT_PORT, FILES);

    // 4. Send the message
    if (send(sock, message, strlen(message), 0) < 0) {
        perror("Send failed");
        return 1;
    }

    printf("[Storage Server] Registration message sent.\n");

    // 5. Close the socket
    close(sock);

    // TODO: In the future, this server will also need to listen for client connections
    // on its own port (9001 in this case).

    return 0;
}