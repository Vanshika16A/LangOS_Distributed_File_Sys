#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h> // For strcasecmp

#define NAME_SERVER_IP "127.0.0.1"
#define NAME_SERVER_PORT 8080


void handle_ns_command(const char* command_str);
#include "client_SS_helper_functions.c"

char username[50];
int ns_sock = -1; // MODIFIED: Global socket for persistent NS connection

// MODIFIED: This function replaces NS_comms. It handles the logic
// for a persistent connection.
void handle_ns_command(const char* command_str) {
    char server_reply[MAX_RESPONSE_LEN];
    
    // --- 1. Send command on persistent socket ---
    if (send(ns_sock, command_str, strlen(command_str), 0) < 0) {
        perror("Send to NS failed");
        ns_sock = -1; // Mark socket as dead
        return;
    }

    // --- 2. Receive loop for full response ---
    int total_read = 0;
    int read_size;
    server_reply[0] = '\0';
    
    while ((read_size = recv(ns_sock, server_reply + total_read, MAX_RESPONSE_LEN - total_read - 1, 0)) > 0) {
        total_read += read_size;
        server_reply[total_read] = '\0';
        
        // Check if the full response has ended
        if (strstr(server_reply, "__END__\n")) {
            break;
        }
    }

    if (read_size <= 0) {
        perror("recv from NS failed");
        ns_sock = -1; // Mark socket as dead
        return;
    }

    // --- 3. Process the response ---
    char* end_token = strstr(server_reply, "__END__");
    if (end_token != NULL) {
        *end_token = '\0';
    }
    
    char* response_type = strtok(server_reply, ";\n");
    if (response_type == NULL) return; // Empty response

    char* ip, *port_str, *filename, *sent_num_str;

    if (strcmp(response_type, "ERROR") == 0) {
        // This is a new standardized error message
        char* error_code_str = strtok(NULL, ";");
        char* error_message = strtok(NULL, "\n"); // Get the rest
        if (error_code_str && error_message) {
            printf("Error [%s]: %s\n", error_code_str, error_message);
        } else {
            // This handles cases where the error from the server might not be formatted correctly
            // or for older, non-standardized error messages.
            printf("%s\n", response_type);
        }
    }
    else if (strcmp(response_type, "REDIRECT_READ") == 0) {
        // strtok was already called, so we start with NULL
        ip = strtok(NULL, ";");
        port_str = strtok(NULL, ";");
        filename = strtok(NULL, ";\n");
        if (ip && port_str && filename) {
            handle_ss_read(ip, atoi(port_str), filename);
        }
    }
    else if (strcmp(response_type, "REDIRECT_WRITE") == 0) {
        ip = strtok(NULL, ";");
        port_str = strtok(NULL, ";");
        filename = strtok(NULL, ";\n");
        sent_num_str = strtok(NULL, ";\n");
        if (ip && port_str && filename && sent_num_str) {
            handle_ss_write_session(ip, atoi(port_str), filename, atoi(sent_num_str));
        }
    }
    else if (strcmp(response_type, "REDIRECT_STREAM") == 0) {
        ip = strtok(NULL, ";");
        port_str = strtok(NULL, ";");
        filename = strtok(NULL, ";\n");
        if (ip && port_str && filename) {
            handle_ss_stream(ip, atoi(port_str), filename);
        }
    }
    else {
        // Not a redirect or a standard error, just a simple message
        // The first token is the message itself.
        printf("%s\n", response_type);
    }
}


int main() {
    char message[MAX_RESPONSE_LEN];

    // --- 1. Initial Connection and Registration ---
    struct sockaddr_in server_addr;
    ns_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ns_sock == -1) {
        perror("Could not create socket");
        return 1;
    }

    server_addr.sin_addr.s_addr = inet_addr(NAME_SERVER_IP);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NAME_SERVER_PORT);

    if (connect(ns_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) { 
        perror("Connect to NS failed");
        close(ns_sock);
        return 1;
    }
    printf("Connected to Name Server.\n");

    printf("Enter your username: ");
    fgets(username, 50, stdin);
    username[strcspn(username, "\n")] = 0;
    snprintf(message, sizeof(message), "REGISTER_CLIENT;%s\n", username);
    
    printf("Registering with server...\n");
    // Use the new command handler for registration
    handle_ns_command(message);
    
    printf("\nSuccessfully registered as '%s'. Type 'exit' to quit.\n", username);
    printf("------------------------------------------------------\n");

    // --- 2. Main interactive loop ---
    char input[MAX_RESPONSE_LEN];
    while (1) {
        // Check if socket died from a previous error
        if (ns_sock == -1) {
            printf("Connection to Name Server lost. Exiting.\n");
            break;
        }

        printf("> ");
        if (fgets(input, MAX_RESPONSE_LEN, stdin) == NULL) {
            break;
        }
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "exit") == 0) {
            break;
        }

        char command_to_send[MAX_RESPONSE_LEN];
        char input_copy[MAX_RESPONSE_LEN];
        strcpy(input_copy, input);

        char* command = strtok(input_copy, " ");
        if (command == NULL) continue;

        // (Command parsing logic is unchanged)
        if (strcasecmp(command, "VIEW") == 0) {
            char* flags = strtok(NULL, " ");
            snprintf(command_to_send, sizeof(command_to_send), "VIEW;%s\n", flags ? flags : "-");
        } 
        else if (strcasecmp(command, "LIST") == 0) {
            snprintf(command_to_send, sizeof(command_to_send), "LIST_USERS;\n");
        }
        else if (strcasecmp(command, "CREATE") == 0) {
            char* filename = strtok(NULL, " ");
            if (!filename) { printf("Usage: CREATE <filename>\n"); continue; }
            snprintf(command_to_send, sizeof(command_to_send), "CREATE;%s\n", filename);
        }
        else if (strcasecmp(command, "READ") == 0) {
            char* filename = strtok(NULL, " ");
            if (!filename) { printf("Usage: READ <filename>\n"); continue; }
            snprintf(command_to_send, sizeof(command_to_send), "READ;%s\n", filename);
        }
        else if (strcasecmp(command, "WRITE") == 0) {
            char* filename = strtok(NULL, " ");
            char* sent_num = strtok(NULL, " ");
            if (!filename || !sent_num) { printf("Usage: WRITE <filename> <sentence_number>\n"); continue; }
            snprintf(command_to_send, sizeof(command_to_send), "WRITE;%s;%s\n", filename, sent_num);
        }
        else if (strcasecmp(command, "DELETE") == 0) {
            char* filename = strtok(NULL, " ");
            if (!filename) { printf("Usage: DELETE <filename>\n"); continue; }
            snprintf(command_to_send, sizeof(command_to_send), "DELETE;%s\n", filename);
        }
        else if (strcasecmp(command, "STREAM") == 0) {
            char* filename = strtok(NULL, " ");
            if (!filename) { printf("Usage: STREAM <filename>\n"); continue; }
            snprintf(command_to_send, sizeof(command_to_send), "STREAM;%s\n", filename);
        }
        else if (strcasecmp(command, "UNDO") == 0) {
            char* filename = strtok(NULL, " ");
            if (!filename) { printf("Usage: UNDO <filename>\n"); continue; }
            snprintf(command_to_send, sizeof(command_to_send), "UNDO;%s\n", filename);
        }
        else if (strcasecmp(command, "INFO") == 0) {
            char* filename = strtok(NULL, " ");
            if (!filename) { printf("Usage: INFO <filename>\n"); continue; }
            snprintf(command_to_send, sizeof(command_to_send), "INFO;%s\n", filename);
        }
        else if (strcasecmp(command, "ADDACCESS") == 0) {
            char* filename = strtok(NULL, " ");
            char* user = strtok(NULL, " ");
            char* perm = strtok(NULL, " ");
            if (!filename || !user || !perm) { printf("Usage: ADDACCESS <filename> <user> <R|W>\n"); continue; }
            snprintf(command_to_send, sizeof(command_to_send), "ADDACCESS;%s;%s;%s\n", filename, user, perm);
        }
        else if (strcasecmp(command, "REMACCESS") == 0) {
            char* filename = strtok(NULL, " ");
            char* user = strtok(NULL, " ");
            if (!filename || !user) { printf("Usage: REMACCESS <filename> <user>\n"); continue; }
            snprintf(command_to_send, sizeof(command_to_send), "REMACCESS;%s;%s\n", filename, user);
        }
        else if (strcasecmp(command, "EXEC") == 0) {
            char* filename = strtok(NULL, " ");
            if (!filename)
            {
                printf("Usage: EXEC <filename>\n");
                continue;
            }
            snprintf(command_to_send, sizeof(command_to_send), "EXEC;%s\n", filename);
        }
        else {
            printf("Unknown command: %s\n", command);
            continue;
        }

        // MODIFIED: Call the new persistent command handler
        handle_ns_command(command_to_send);
    }

    // --- 3. Close persistent connection ---
    if (ns_sock != -1) {
        close(ns_sock);
    }
    printf("Disconnected from Name Server.\n");
    return 0;
}