#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define NAME_SERVER_IP "127.0.0.1"
#define NAME_SERVER_PORT 8080
#define MAX_USERNAME_LEN 1024
#define MAX_RESPONSE_LEN 4096
// Helper function to handle communication for one command
void send_command_and_receive_response(const char* command_str) {
    int sock;
    struct sockaddr_in server_addr;
    char server_reply[MAX_RESPONSE_LEN];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Could not create socket");
        return;
    }

    server_addr.sin_addr.s_addr = inet_addr(NAME_SERVER_IP);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NAME_SERVER_PORT);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(sock);
        return;
    }

    // Send command
    if (send(sock, command_str, strlen(command_str), 0) < 0) {
        perror("Send failed");
        close(sock);
        return;
    }

    // Receive response until "__END__"
    int read_size;
    while ((read_size = recv(sock, server_reply, MAX_RESPONSE_LEN - 1, 0)) > 0) {
        server_reply[read_size] = '\0';
        // Check if the end token is in this chunk
        char* end_token = strstr(server_reply, "__END__");
        if (end_token != NULL) {
            *end_token = '\0'; // Terminate the string before the token
        }
        
        printf("%s", server_reply);

        if (end_token != NULL) {
            break; // We've found the end
        }
    }

    close(sock);
}


int main() {
    char username[50];
    char message[MAX_RESPONSE_LEN];

    // 1. Initial Registration
    printf("Enter your username: ");
    fgets(username, 50, stdin);
    username[strcspn(username, "\n")] = 0;

   snprintf(message, sizeof(message), "REGISTER_CLIENT;%s\n", username);
    printf("Registering with server...\n");
    send_command_and_receive_response(message); // Now we will see the ACK
    printf("\nSuccessfully registered as '%s'. Type 'exit' to quit.\n", username);
    printf("------------------------------------------------------\n");

    // 2. Main interactive loop
    char input[MAX_RESPONSE_LEN];
    while (1) {
        printf("> ");
        if (fgets(input, MAX_RESPONSE_LEN, stdin) == NULL) {
            break;
        }
        input[strcspn(input, "\n")] = 0; // Remove newline

        if (strcmp(input, "exit") == 0) {
            break;
        }

        // 4. Parse input and format command for protocol
        char command_to_send[MAX_RESPONSE_LEN];
        char input_copy[MAX_RESPONSE_LEN];
        strcpy(input_copy, input);

        char* command = strtok(input_copy, " ");
        if (command == NULL) continue;

        if (strcasecmp(command, "VIEW") == 0) {
            char* flags = strtok(NULL, " ");
            snprintf(command_to_send, sizeof(command_to_send), "VIEW;%s\n", flags ? flags : "-");
        } 
        else if (strcasecmp(command, "LIST") == 0) {
            snprintf(command_to_send, sizeof(command_to_send), "LIST_USERS;\n");
        }
        // ... add parsing for INFO, ADDACCESS etc. here ...
        else {
            printf("Unknown command: %s\n", command);
            continue;
        }

        // 5. Send and receive
        send_command_and_receive_response(command_to_send);
    }

    return 0;
}