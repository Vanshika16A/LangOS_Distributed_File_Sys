#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define NAME_SERVER_IP "127.0.0.1"
#define NAME_SERVER_PORT 8080
#define MAX_USERNAME_LEN 1024
#define MAX_RESPONSE_LEN 8192

// connection to the storage server (no change)
int connect_to_ss(const char* ip, int port) { 
    int sock;
    struct sockaddr_in ss_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Could not create SS socket");
        return -1;
    }

    ss_addr.sin_addr.s_addr = inet_addr(ip);
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&ss_addr, sizeof(ss_addr)) < 0) { 
        perror("SS Connect failed");
        close(sock);
        return -1;
    }
    return sock;
}

// read_from_ss (no change)
void read_from_ss(int sock) {
    char ss_reply[MAX_RESPONSE_LEN];
    int read_size;
    while ((read_size = recv(sock, ss_reply, MAX_RESPONSE_LEN - 1, 0)) > 0) {
        ss_reply[read_size] = '\0';
        char* end_token = strstr(ss_reply, "__SS_END__");
        if (end_token != NULL) {
            *end_token = '\0';
        }
        printf("%s", ss_reply);
        if (end_token != NULL) {
            break;
        }
    }
}

// REMOVED: handle_ss_create
// Reason: This is now handled by the Name Server.

// Handles the SS_READ operation (no change)
void handle_ss_read(const char* ip, int port, const char* filename) {
    int ss_sock = connect_to_ss(ip, port);
    if (ss_sock < 0) return;
    
    char command[1024];
    snprintf(command, sizeof(command), "SS_READ;%s\n", filename);
    send(ss_sock, command, strlen(command), 0);
    read_from_ss(ss_sock);
    close(ss_sock);
}

// Handles the stateful SS_WRITE session (no change)
void handle_ss_write_session(const char* ip, int port, const char* filename, int sentence_num) {
    int ss_sock = connect_to_ss(ip, port);
    if (ss_sock < 0) return;
    
    char command[1024];
    char ss_reply[128];
    int read_size;
    
    snprintf(command, sizeof(command), "SS_LOCK_SENTENCE;%s;%d\n", filename, sentence_num);
    send(ss_sock, command, strlen(command), 0);
    
    read_size = recv(ss_sock, ss_reply, 127, 0);
    ss_reply[read_size] = '\0';
    
    if (strncmp(ss_reply, "ACK_LOCK", 8) != 0) {
        printf("Error: Could not acquire lock from storage server: %s\n", ss_reply);
        close(ss_sock);
        return;
    }
    
    printf("Lock acquired. Enter <word_index> <content> or 'ETIRW' to finish.\n");
    
    char input[MAX_RESPONSE_LEN];
    while (1) {
        printf("write> ");
        if (fgets(input, MAX_RESPONSE_LEN, stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;

        if (strcasecmp(input, "ETIRW") == 0) {
            send(ss_sock, "COMMIT_WRITE;\n", 15, 0);
            read_from_ss(ss_sock); // Read final ACK_COMMIT__SS_END__
            snprintf(command, sizeof(command), "UPDATE_META;%s\n", filename);   
            handle_ns_command(command);
            break;
        }
        
        char* index_str = strtok(input, " ");
        char* content = strtok(NULL, ""); 
        
        if (!index_str || !content) {
            printf("Invalid format. Use: <word_index> <content>\n");
            continue;
        }
        
        snprintf(command, sizeof(command), "WRITE_DATA;%d;%s\n", atoi(index_str), content);
        send(ss_sock, command, strlen(command), 0);
        
        read_size = recv(ss_sock, ss_reply, 127, 0);
        ss_reply[read_size] = '\0';
        if (strncmp(ss_reply, "ACK_DATA", 8) != 0) {
            printf("Error: Write data not acknowledged.\n");
            break;
        }
    }
    
    close(ss_sock);
    printf("Write session finished.\n");
}
void stream_from_ss(int ss_sock, const char* filename) {
    char* total_reply = malloc(MAX_RESPONSE_LEN * 10); // 40KB buffer
    if (total_reply == NULL) {
        perror("malloc failed");
        close(ss_sock);
        return;
    }
    total_reply[0] = '\0'; // Start with an empty string
    
    char recv_buf[MAX_RESPONSE_LEN];
    int read_size;
    char* end_token = NULL;

    // 2. Read loop: receive all data from SS into the single 'total_reply' buffer.
    while ((read_size = recv(ss_sock, recv_buf, MAX_RESPONSE_LEN - 1, 0)) > 0) {
        recv_buf[read_size] = '\0';
        
        // Check for end token *before* concatenating
        end_token = strstr(recv_buf, "__SS_END__");
        if (end_token != NULL) {
            *end_token = '\0'; // Terminate the buffer before the token
        }
        
        // Append the received chunk to our total buffer
        strncat(total_reply, recv_buf, MAX_RESPONSE_LEN * 10 - strlen(total_reply) - 1);

        if (end_token != NULL) {
            break; // We found the end, stop reading
        }
    }
    close(ss_sock); // We have all the data, close the socket.

    // 3. Now, parse the *complete* text and stream it word-by-word
    printf("[Streaming file: %s...]\n", filename);
    char* word = strtok(total_reply, " \t\n\r"); // Use all whitespace delimiters
    
    while (word != NULL) {
        printf("%s ", word);
        fflush(stdout);  // Force the word to print *now*
        usleep(100000);  // 0.1 second delay
        word = strtok(NULL, " \t\n\r");
    }
    printf("\n[...Stream finished]\n");

    free(total_reply); // Clean up the buffer
}
void handle_ss_stream(const char* ip, int port, const char* filename) {
    char cmd[1024];
    int ss_sock = connect_to_ss(ip, port);
    if (ss_sock < 0)
        return;
    snprintf(cmd, sizeof(cmd), "SS_STREAM;%s\n", filename);
    send(ss_sock, cmd, strlen(cmd), 0);
    stream_from_ss(ss_sock, filename);
}






//TODO: combine READING and STREAMING logic into a single read_from function [is it efficient tho]