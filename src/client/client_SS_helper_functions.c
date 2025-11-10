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
//connection to the storage server
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
void handle_ss_create(const char* ip, int port, const char* filename) {
    int ss_sock = connect_to_ss(ip, port);
    if (ss_sock < 0) return;
    
    char command[1024];
    snprintf(command, sizeof(command), "SS_CREATE;%s\n", filename);
    send(ss_sock, command, strlen(command), 0);
    read_from_ss(ss_sock);
    close(ss_sock);
}

// Handles the SS_READ operation
void handle_ss_read(const char* ip, int port, const char* filename) {
    int ss_sock = connect_to_ss(ip, port);
    if (ss_sock < 0) return;
    
    char command[1024];
    snprintf(command, sizeof(command), "SS_READ;%s\n", filename);
    send(ss_sock, command, strlen(command), 0);
    read_from_ss(ss_sock);
    close(ss_sock);
}

// Handles the stateful SS_WRITE session
void handle_ss_write_session(const char* ip, int port, const char* filename, int sentence_num) {
    int ss_sock = connect_to_ss(ip, port);
    if (ss_sock < 0) return;
    
    char command[1024];
    char ss_reply[128];
    int read_size;
    
    // 1. Send lock command
    snprintf(command, sizeof(command), "SS_LOCK_SENTENCE;%s;%d\n", filename, sentence_num);
    send(ss_sock, command, strlen(command), 0);
    
    // Wait for ACK_LOCK (this is a blocking read)
    read_size = recv(ss_sock, ss_reply, 127, 0);
    ss_reply[read_size] = '\0';
    
    if (strncmp(ss_reply, "ACK_LOCK", 8) != 0) {
        printf("Error: Could not acquire lock from storage server.\n");
        close(ss_sock);
        return;
    }
    
    printf("Lock acquired. Enter <word_index> <content> or 'ETIRW' to finish.\n");
    
    // 2. Write loop
    char input[MAX_RESPONSE_LEN];
    while (1) {
        printf("write> ");
        if (fgets(input, MAX_RESPONSE_LEN, stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0; // Remove newline

        if (strcasecmp(input, "ETIRW") == 0) {
            send(ss_sock, "COMMIT_WRITE;\n", 15, 0);
            read_from_ss(ss_sock); // Read final ACK_COMMIT__SS_END__
            break;
        }
        
        // Parse "index content"
        char* index_str = strtok(input, " ");
        char* content = strtok(NULL, ""); // Get rest of the line
        
        if (!index_str || !content) {
            printf("Invalid format. Use: <word_index> <content>\n");
            continue;
        }
        
        snprintf(command, sizeof(command), "WRITE_DATA;%d;%s\n", atoi(index_str), content);
        send(ss_sock, command, strlen(command), 0);
        
        // Wait for ACK_DATA
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

// Handles the SS_DELETE operation
// Returns 1 on success, 0 on failure
int handle_ss_delete(const char* ip, int port, const char* filename) {
    int ss_sock = connect_to_ss(ip, port);
    if (ss_sock < 0) return 0;
    
    char command[1024];
    snprintf(command, sizeof(command), "SS_DELETE;%s\n", filename);
    send(ss_sock, command, strlen(command), 0);
    
    char ss_reply[MAX_RESPONSE_LEN];
    int read_size = recv(ss_sock, ss_reply, MAX_RESPONSE_LEN - 1, 0);
    ss_reply[read_size] = '\0';
    close(ss_sock);
    
    if (strstr(ss_reply, "ACK_DELETE")) {
        return 1;
    } else {
        printf("Storage Server Error: %s\n", ss_reply);
        return 0;
    }

}