#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define NAME_SERVER_IP "127.0.0.1"
#define NAME_SERVER_PORT 8080

// This SS's details
#define SS_IP "127.0.0.1"
#define SS_PORT 9001
#define SS_ROOT_DIR "ss_files" // Directory to store files

#define MAX_BUFFER 2048
#define MAX_SENTENCE_LEN 1024
#define MAX_WORDS 256

// Struct to pass to connection handler thread
typedef struct {
    int client_socket;
} connection_t;

// Struct for buffering writes
typedef struct WriteOp {
    int word_index;
    char content[1024];
    struct WriteOp* next;
} WriteOp;

// Global mutex to protect the file system during commits
pthread_mutex_t file_system_mutex = PTHREAD_MUTEX_INITIALIZER;

// Helper: Register this SS with the Name Server
void register_with_name_server() {
    int sock;
    struct sockaddr_in ns_addr;
    char message[1024];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Could not create socket");
        exit(1);
    }

    ns_addr.sin_addr.s_addr = inet_addr(NAME_SERVER_IP);
    ns_addr.sin_family = AF_INET;
    ns_addr.sin_port = htons(NAME_SERVER_PORT);

    if (connect(sock, (struct sockaddr*)&ns_addr, sizeof(ns_addr)) < 0) {
        perror("Connect to Name Server failed");
        exit(1);
    }

    printf("[Storage Server] Connected to Name Server.\n");
    snprintf(message, sizeof(message), "REGISTER_SS;%s;%d\n", SS_IP, SS_PORT);

    if (send(sock, message, strlen(message), 0) < 0) {
        perror("Send failed");
    } else {
        printf("[Storage Server] Registration message sent.\n");
        char response[64];
        recv(sock, response, 64, 0); // Wait for ACK
    }
    close(sock);
}

// Helper: Ensure file path is safe and within our directory
void get_safe_path(const char* filename, char* path_buffer) {
    snprintf(path_buffer, 256, "%s/%s", SS_ROOT_DIR, filename);
    // Basic check to prevent ".."
    if (strstr(filename, "..")) {
        path_buffer[0] = '\0';
    }
}

// Helper: Get the Nth sentence from file content
char* get_nth_sentence(char* content, int n, char** end_ptr) {
    char* start = content;
    for (int i = 0; i < n; i++) {
        start = strpbrk(start, ".?!");
        if (!start) return NULL; // Not enough sentences
        start++; // Move past the delimiter
        // Skip whitespace
        while (*start == ' ' || *start == '\n' || *start == '\t' || *start == '\r') {
            start++;
        }
    }
    
    *end_ptr = strpbrk(start, ".?!");
    if (*end_ptr) {
        (*end_ptr)++; // Include the delimiter
    }
    return start;
}

// Helper: Apply buffered writes to the file
void commit_changes(const char* filename, int sentence_num, WriteOp* write_head) {
    char filepath[256];
    get_safe_path(filename, filepath);
    if (!filepath[0]) return; // Invalid path

    // Lock the entire file system for this operation
    pthread_mutex_lock(&file_system_mutex);

    FILE* f_read = fopen(filepath, "r");
    if (!f_read) {
        pthread_mutex_unlock(&file_system_mutex);
        return;
    }
    
    // Read entire file
    fseek(f_read, 0, SEEK_END);
    long f_size = ftell(f_read);
    fseek(f_read, 0, SEEK_SET);
    char* file_content = malloc(f_size + 1);
    fread(file_content, f_size, 1, f_read);
    file_content[f_size] = '\0';
    fclose(f_read);

    char* sentence_end = NULL;
    char* sentence_start = get_nth_sentence(file_content, sentence_num, &sentence_end);
    
    if (!sentence_start) {
        printf("Error: Sentence %d not found.\n", sentence_num);
        free(file_content);
        pthread_mutex_unlock(&file_system_mutex);
        return;
    }

    // Isolate the sentence
    long sentence_len;
    if (sentence_end) {
        sentence_len = sentence_end - sentence_start;
    } else {
        sentence_len = strlen(sentence_start); // Last sentence
    }

    char* sentence = strndup(sentence_start, sentence_len);
    
    // Split sentence into words
    char* words[MAX_WORDS];
    int word_count = 0;
    char* word = strtok(sentence, " \t\n\r");
    while(word && word_count < MAX_WORDS) {
        words[word_count++] = strdup(word);
        word = strtok(NULL, " \t\n\r");
    }

    // Apply changes
    WriteOp* current_op = write_head;
    while(current_op) {
        if (current_op->word_index >= 0 && current_op->word_index < word_count) {
            free(words[current_op->word_index]); // Free old word
            words[current_op->word_index] = strdup(current_op->content);
        } else {
            printf("Warn: Word index %d out of bounds.\n", current_op->word_index);
        }
        current_op = current_op->next;
    }

    // Rebuild the sentence
    char new_sentence[MAX_SENTENCE_LEN] = "";
    for (int i = 0; i < word_count; i++) {
        strcat(new_sentence, words[i]);
        // Add space, but not for the last word before the delimiter
        if (i < word_count - 1 && (strlen(words[i+1]) > 1 || !strpbrk(words[i+1], ".?!"))) {
            strcat(new_sentence, " ");
        }
    }

    // Rebuild the entire file content
    char tmp_filepath[260];
    snprintf(tmp_filepath, sizeof(tmp_filepath), "%s.tmp", filepath);
    FILE* f_write = fopen(tmp_filepath, "w");
    
    // Write content before the sentence
    long before_len = sentence_start - file_content;
    fwrite(file_content, 1, before_len, f_write);
    
    // Write new sentence
    fwrite(new_sentence, 1, strlen(new_sentence), f_write);

    // Write content after the sentence
    if (sentence_end) {
        fwrite(sentence_end, 1, strlen(sentence_end), f_write);
    }
    
    fclose(f_write);
    
    // Atomically replace the old file
    rename(tmp_filepath, filepath);

    // Cleanup
    free(file_content);
    free(sentence);
    for (int i = 0; i < word_count; i++) free(words[i]);
    
    pthread_mutex_unlock(&file_system_mutex);
}

// Handle client connections
void* handle_ss_connection(void* arg) {
    connection_t* conn = (connection_t*)arg;
    int sock = conn->client_socket;
    free(conn);
    
    char buffer[MAX_BUFFER];
    int read_size;
    
    // For WRITE sessions
    WriteOp* write_head = NULL;
    int locked_sentence_num = -1;
    char locked_filename[256] = "";

    // Read loop
    while((read_size = recv(sock, buffer, MAX_BUFFER - 1, 0)) > 0) {
        buffer[read_size] = '\0';
        char* command = strtok(buffer, ";\n");
        if (!command) continue;

        printf("[SS] Received Command: %s\n", command);

        if (strcmp(command, "SS_CREATE") == 0) {
            char* filename = strtok(NULL, ";\n");
            char filepath[256];
            get_safe_path(filename, filepath);
            
            if (filepath[0]) {
                int fd = open(filepath, O_WRONLY | O_CREAT | O_EXCL, 0644);
                if (fd == -1) {
                    send(sock, "ERROR: File exists or cannot create\n__SS_END__\n", 43, 0);
                } else {
                    close(fd);
                    send(sock, "ACK_CREATE\n__SS_END__\n", 21, 0);
                }
            }
        } 
        else if (strcmp(command, "SS_READ") == 0) {
            char* filename = strtok(NULL, ";\n");
            char filepath[256];
            get_safe_path(filename, filepath);
            
            FILE* f = fopen(filepath, "r");
            if (!f) {
                send(sock, "ERROR: File not found\n__SS_END__\n", 30, 0);
            } else {
                char file_buf[1024];
                size_t nbytes;
                while((nbytes = fread(file_buf, 1, 1024, f)) > 0) {
                    send(sock, file_buf, nbytes, 0);
                }
                fclose(f);
                send(sock, "\n__SS_END__\n", 12, 0);
            }
        }
        else if (strcmp(command, "SS_DELETE") == 0) {
            char* filename = strtok(NULL, ";\n");
            char filepath[256];
            get_safe_path(filename, filepath);
            
            if (remove(filepath) == 0) {
                send(sock, "ACK_DELETE\n__SS_END__\n", 21, 0);
            } else {
                send(sock, "ERROR: Could not delete\n__SS_END__\n", 30, 0);
            }
        }
        else if (strcmp(command, "SS_LOCK_SENTENCE") == 0) {
            // This is a coarse-grained lock for demo.
            // A real system would have finer locks.
            char* filename = strtok(NULL, ";\n");
            char* sent_num_str = strtok(NULL, ";\n");
            
            // TODO: Implement actual per-sentence locking
            
            strcpy(locked_filename, filename);
            locked_sentence_num = atoi(sent_num_str);
            printf("[SS] File '%s' sentence %d locked (demo lock)\n", locked_filename, locked_sentence_num);
            send(sock, "ACK_LOCK\n", 9, 0); // Send ACK (no __SS_END__)
        }
        else if (strcmp(command, "WRITE_DATA") == 0) {
            int word_idx = atoi(strtok(NULL, ";\n"));
            char* content = strtok(NULL, ";\n");
            
            WriteOp* new_op = (WriteOp*)malloc(sizeof(WriteOp));
            new_op->word_index = word_idx;
            strcpy(new_op->content, content);
            new_op->next = write_head;
            write_head = new_op;
            
            printf("[SS] Buffered write: idx %d, content '%s'\n", word_idx, content);
            send(sock, "ACK_DATA\n", 9, 0); // Send ACK
        }
        else if (strcmp(command, "COMMIT_WRITE") == 0) {
            printf("[SS] Committing changes to '%s', sentence %d\n", locked_filename, locked_sentence_num);
            commit_changes(locked_filename, locked_sentence_num, write_head);
            
            // Clean up buffer
            while(write_head) {
                WriteOp* temp = write_head;
                write_head = write_head->next;
                free(temp);
            }
            locked_sentence_num = -1;
            locked_filename[0] = '\0';
            
            send(sock, "ACK_COMMIT\n__SS_END__\n", 21, 0); // End of session
        }
    }
    
    // Clean up if client disconnected mid-write
    while(write_head) {
        WriteOp* temp = write_head;
        write_head = write_head->next;
        free(temp);
    }
    printf("[SS] Client disconnected.\n");
    close(sock);
    return NULL;
}


int main() {
    // Create root directory for files if it doesn't exist
    mkdir(SS_ROOT_DIR, 0755);
    
    // 1. Register with Name Server
    register_with_name_server();

    // 2. Start listening for client connections
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Could not create SS socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SS_PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("SS Bind failed");
        return 1;
    }
    printf("[Storage Server] Bind successful on port %d.\n", SS_PORT);

    listen(server_sock, 10);
    printf("[Storage Server] Waiting for client connections...\n");

    while ((client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len))) {
        printf("[SS] Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        pthread_t thread_id;
        connection_t* conn = (connection_t*)malloc(sizeof(connection_t));
        conn->client_socket = client_sock;

        if (pthread_create(&thread_id, NULL, handle_ss_connection, (void*)conn) < 0) {
            perror("could not create thread");
            free(conn);
        }
    }

    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;
}