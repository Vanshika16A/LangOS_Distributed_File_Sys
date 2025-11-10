#include "client_SS_helper_functions.c"
char username[50];
// Helper function to handle comms for one command
void NS_comms(const char* command_str) { //A universal command handler function
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

    // Receives one packet
    int read_size = recv(sock, server_reply, MAX_RESPONSE_LEN - 1, 0);
    if (read_size <= 0) {
        perror("recv failed");
        close(sock);
        return;
    }
    server_reply[read_size] = '\0';

    // terminates at __END__
    char* end_token = strstr(server_reply, "__END__");
    if (end_token != NULL) {
        *end_token = '\0';
    }
    close(sock); 
    
    // processes that one packet
   
    char* ip, *port_str, *filename, *sent_num_str;
    
    if (strncmp(server_reply, "REDIRECT_CREATE", 15) == 0) {
        strtok(server_reply, ";"); // Skip "REDIRECT_CREATE"
        ip = strtok(NULL, ";");
        port_str = strtok(NULL, ";");
        filename = strtok(NULL, ";\n");
        if (ip && port_str && filename) {
            handle_ss_create(ip, atoi(port_str), filename);
        }
    } 
    else if (strncmp(server_reply, "REDIRECT_READ", 13) == 0) {
        strtok(server_reply, ";");
        ip = strtok(NULL, ";");
        port_str = strtok(NULL, ";");
        filename = strtok(NULL, ";\n");
        if (ip && port_str && filename) {
            handle_ss_read(ip, atoi(port_str), filename);
        }
    }
    else if (strncmp(server_reply, "REDIRECT_WRITE", 14) == 0) {
        strtok(server_reply, ";");
        ip = strtok(NULL, ";");
        port_str = strtok(NULL, ";");
        filename = strtok(NULL, ";\n");
        sent_num_str = strtok(NULL, ";\n");
        if (ip && port_str && filename && sent_num_str) {
            handle_ss_write_session(ip, atoi(port_str), filename, atoi(sent_num_str));
        }
    }
    else if (strncmp(server_reply, "REDIRECT_DELETE", 15) == 0) {
        char ns_command[1024];
        strtok(server_reply, ";");
        ip = strtok(NULL, ";");
        port_str = strtok(NULL, ";");
        filename = strtok(NULL, ";\n");
        if (ip && port_str && filename) {
            if (handle_ss_delete(ip, atoi(port_str), filename)) { //handles the case in which the deletion was successfull
                printf("File deleted from SS, updating Name Server...\n");
                snprintf(ns_command, sizeof(ns_command), "DELETE_METADATA;%s\n", filename);
                NS_comms(ns_command); // Recursive call
            }
        }
    }
    else {
        // Not a redirect, just a simple message from NS
        printf("%s\n", server_reply);
    }
}


int main() {
    char message[MAX_RESPONSE_LEN];

    // 1. Initial Registration
    printf("Enter your username: ");
    fgets(username, 50, stdin);
    username[strcspn(username, "\n")] = 0;
    snprintf(message, sizeof(message), "REGISTER_CLIENT;%s\n", username);
    printf("Registering with server...\n");
    NS_comms(message); // Now we will see the ACK
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
        //to implement: UNDO, INFO, STREAM, ACCESS, EXEC
        else {
            printf("Unknown command: %s\n", command);
            continue;
        }

        // send and receive packet
        NS_comms(command_to_send);
    }

    return 0;
}