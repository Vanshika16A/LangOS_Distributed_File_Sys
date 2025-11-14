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
// ADDED: For scanning directory
#include <dirent.h>

#define NAME_SERVER_IP "127.0.0.1"
#define NAME_SERVER_PORT 8080

// This SS's details
#define SS_IP "127.0.0.1"
#define SS_PORT 9001 
// MODIFIED: This port must now accept connections from BOTH
// clients (for READ/WRITE) and the NS (for CREATE/DELETE).
#define SS_ROOT_DIR "ss_files" 

#define MAX_BUFFER 2048
#define MAX_SENTENCE_LEN 1024
#define MAX_WORDS 256
// ADDED: For building file list
#define MAX_FILE_LIST_LEN 4096


typedef struct {
    int conn_socket; // MODIFIED: Renamed for clarity
} connection_t;

// ... (WriteOp struct, file_system_mutex, get_safe_path, 
//    get_nth_sentence, commit_changes are all unchanged) ...
typedef struct WriteOp {
    int word_index;
    char content[1024];
    struct WriteOp* next;
} WriteOp;

pthread_mutex_t file_system_mutex = PTHREAD_MUTEX_INITIALIZER;

void get_safe_path(const char* filename, char* path_buffer) {
    snprintf(path_buffer, 256, "%s/%s", SS_ROOT_DIR, filename);
    if (strstr(filename, "..")) {
        path_buffer[0] = '\0';
    }
}

char* get_nth_sentence(char* content, int n, char** end_ptr) {
    char* start = content;
    for (int i = 0; i < n; i++) {
        start = strpbrk(start, ".?!");
        if (!start) return NULL; 
        start++; 
        while (*start == ' ' || *start == '\n' || *start == '\t' || *start == '\r') {
            start++;
        }
    }
    
    *end_ptr = strpbrk(start, ".?!");
    if (*end_ptr) {
        (*end_ptr)++; 
    }
    return start;
}

void commit_changes(const char* filename, int sentence_num, WriteOp* write_head) {
    char filepath[256];
    get_safe_path(filename, filepath);
    if (!filepath[0]) return;

    char backup_path[256];
    snprintf(backup_path, sizeof(backup_path), "%s.bak", filepath);

    pthread_mutex_lock(&file_system_mutex);

    // --- FIX 1: Handle Empty/New Files ---
    FILE* f_read = fopen(filepath, "r");
    char* file_content = NULL;
    long f_size = 0;

    if (!f_read) {
        // This is OK. It just means the file is new and empty.
        printf("[SS] commit_changes: File not found or empty. Treating as new.\n");
        file_content = strdup(""); // Use an empty string
    } else {
        // File exists, read its content
        fseek(f_read, 0, SEEK_END);
        f_size = ftell(f_read);
        fseek(f_read, 0, SEEK_SET);
        file_content = malloc(f_size + 1);
        fread(file_content, f_size, 1, f_read);
        file_content[f_size] = '\0';
        fclose(f_read); // f_read is now our flag for "did file exist before?"
    }
    
    char* sentence_end = NULL;
    char* sentence_start = get_nth_sentence(file_content, sentence_num, &sentence_end);
    
    // --- FIX 2: Handle Sentence 0 on an Empty File ---
    if (!sentence_start) {
        if (sentence_num == 0) {
            sentence_start = file_content; // Point to the empty string ""
        } else {
            printf("Error: Sentence %d not found.\n", sentence_num);
            free(file_content);
            pthread_mutex_unlock(&file_system_mutex);
            return;
        }
    }

    long sentence_len;
    if (sentence_end) {
        sentence_len = sentence_end - sentence_start;
    } else {
        sentence_len = strlen(sentence_start);
    }
    char* sentence = strndup(sentence_start, sentence_len);
    
    // --- FIX 3: Make 'words' array larger to handle new words ---
    char* words[MAX_WORDS * 2]; // Give space for appends
    int word_count = 0;
    
    if (strlen(sentence) > 0) { // Only tokenize if sentence isn't empty
        char* word = strtok(sentence, " \t\n\r");
        while(word && word_count < (MAX_WORDS * 2)) {
            words[word_count++] = strdup(word);
            word = strtok(NULL, " \t\n\r");
        }
    }
    
    // --- FIX 4: Corrected WriteOp logic to handle appends ---
    WriteOp* current_op = write_head;
    while(current_op) {
        if (current_op->word_index >= 0 && current_op->word_index < word_count) {
            // Case 1: Modify existing word
            free(words[current_op->word_index]);
            words[current_op->word_index] = strdup(current_op->content);
        } 
        else if (current_op->word_index == word_count && word_count < (MAX_WORDS * 2)) {
            // Case 2: Append new word to the end
            words[word_count++] = strdup(current_op->content);
        }
        else {
            // Case 3: Invalid index (out of bounds)
            printf("Warn: Word index %d out of bounds (count is %d).\n", current_op->word_index, word_count);
        }
        current_op = current_op->next;
    }

    // Rebuild the sentence (this part was correct)
    char new_sentence[MAX_SENTENCE_LEN] = "";
    for (int i = 0; i < word_count; i++) {
        strcat(new_sentence, words[i]);
        if (i < word_count - 1 && (strlen(words[i+1]) > 1 || !strpbrk(words[i+1], ".?!"))) {
            strcat(new_sentence, " ");
        }
    }

    // Write to .tmp file (this part was correct)
    char tmp_filepath[260];
    snprintf(tmp_filepath, sizeof(tmp_filepath), "%s.tmp", filepath);
    FILE* f_write = fopen(tmp_filepath, "w");
    
    long before_len = sentence_start - file_content;
    fwrite(file_content, 1, before_len, f_write);
    fwrite(new_sentence, 1, strlen(new_sentence), f_write);

    if (sentence_end) {
        fwrite(sentence_end, 1, strlen(sentence_end), f_write);
    }
    
    fclose(f_write);
    
    // --- FIX 5: Only create backup if file existed before ---
    if (f_read != NULL) { // f_read is NULL if file was new
        if (rename(filepath, backup_path) != 0) {
            if(errno != ENOENT) { 
                perror("[SS] Failed to create backup file");
            }
        }
    }

    // Atomic commit (this part was correct)
    if (rename(tmp_filepath, filepath) != 0) {
        perror("[SS] FAILED TO COMMIT. Attempting to restore backup.");
        rename(backup_path, filepath);
    }

    // Cleanup (this part was correct)
    free(file_content);
    free(sentence);
    for (int i = 0; i < word_count; i++) free(words[i]);
    
    pthread_mutex_unlock(&file_system_mutex);
}


// MODIFIED: This function now sends the file list
void register_with_name_server() {
    int sock;
    struct sockaddr_in ns_addr;
    char message[MAX_BUFFER + MAX_FILE_LIST_LEN]; // Make buffer larger
    char file_list[MAX_FILE_LIST_LEN] = "";

    // --- 1. Build File List ---
    DIR *d;
    struct dirent *dir;
    d = opendir(SS_ROOT_DIR);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            // Skip . and ..
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }
            // Add to list
            if (strlen(file_list) == 0) {
                strncpy(file_list, dir->d_name, MAX_FILE_LIST_LEN - 1);
            } else {
                strncat(file_list, ",", MAX_FILE_LIST_LEN - strlen(file_list) - 1);
                strncat(file_list, dir->d_name, MAX_FILE_LIST_LEN - strlen(file_list) - 1);
            }
        }
        closedir(d);
    }
    // --- End Build File List ---


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
    
    // MODIFIED: Send IP, Port, and File List
    // Format: REGISTER_SS;ip;port;file1.txt,file2.txt\n
    snprintf(message, sizeof(message), "REGISTER_SS;%s;%d;%s\n", SS_IP, SS_PORT, file_list);

    if (send(sock, message, strlen(message), 0) < 0) {
        perror("Send failed");
    } else {
        printf("[Storage Server] Registration message sent: %s", message);
        char response[64];
        recv(sock, response, 64, 0); // Wait for ACK
        // TODO: Handle ACK properly
    }
    close(sock);
}


// MODIFIED: Renamed 'sock' to 'conn_socket'
void* handle_ss_connection(void* arg) {
    connection_t* conn = (connection_t*)arg;
    int sock = conn->conn_socket; // Get socket from struct
    free(conn);
    
    char buffer[MAX_BUFFER];
    int read_size;
    
    // (No change to the rest of this function's logic)
    // It already handles SS_CREATE, SS_DELETE, SS_READ, and the WRITE session
    // This function will now be called by connections from
    // EITHER a Client (for READ/WRITE) OR the NS (for CREATE/DELETE).

    WriteOp* write_head = NULL;
    int locked_sentence_num = -1;
    char locked_filename[256] = "";

    while((read_size = recv(sock, buffer, MAX_BUFFER - 1, 0)) > 0) {
        buffer[read_size] = '\0';
        char* command_copy = strdup(buffer); // Copy for safe printing
        char* command = strtok(buffer, ";\n");
        if (!command) {
            free(command_copy);
            continue;
        }

        printf("[SS] Received Command: %s\n", command_copy);
        free(command_copy);

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
        else if (strcmp(command, "SS_STREAM") == 0) {
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
            char* filename = strtok(NULL, ";\n");
            char* sent_num_str = strtok(NULL, ";\n");
            
            // TODO: Implement actual per-sentence locking
            
            strcpy(locked_filename, filename);
            locked_sentence_num = atoi(sent_num_str);
            printf("[SS] File '%s' sentence %d locked (demo lock)\n", locked_filename, locked_sentence_num);
            send(sock, "ACK_LOCK\n", 9, 0); 
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
            send(sock, "ACK_DATA\n", 9, 0); 
        }
        else if (strcmp(command, "COMMIT_WRITE") == 0) {
            printf("[SS] Committing changes to '%s', sentence %d\n", locked_filename, locked_sentence_num);
            commit_changes(locked_filename, locked_sentence_num, write_head);
            
            while(write_head) {
                WriteOp* temp = write_head;
                write_head = write_head->next;
                free(temp);
            }
            locked_sentence_num = -1;
            locked_filename[0] = '\0';
            
            send(sock, "ACK_COMMIT\n__SS_END__\n", 21, 0); 
        }
        else if (strcmp(command, "SS_UNDO") == 0) {
            char* filename = strtok(NULL, ";\n");
            char filepath[256];
            char backup_path[256];
            
            get_safe_path(filename, filepath);
            snprintf(backup_path, sizeof(backup_path), "%s.bak", filepath);

            // Atomically restore the backup by renaming it to the main file
            if (rename(backup_path, filepath) == 0) {
                printf("[SS] File '%s' restored from backup.\n", filename);
                send(sock, "ACK_UNDO\n__SS_END__\n", 19, 0);
            } else {
                perror("[SS] Failed to restore backup");
                send(sock, "ERROR: No backup found or rename failed\n__SS_END__\n", 49, 0);
            }
        }
    }
    
    while(write_head) {
        WriteOp* temp = write_head;
        write_head = write_head->next;
        free(temp);
    }
    printf("[SS] Connection closed.\n");
    close(sock);
    return NULL;
}


int main() {
    mkdir(SS_ROOT_DIR, 0755);
    
    register_with_name_server();

    int server_sock, new_conn_sock; // MODIFIED: Renamed
    struct sockaddr_in server_addr, conn_addr; // MODIFIED: Renamed
    socklen_t conn_len = sizeof(conn_addr); // MODIFIED: Renamed

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
    printf("[Storage Server] Waiting for connections (from Clients or NS)...\n");

    // MODIFIED: Renamed variables
    while ((new_conn_sock = accept(server_sock, (struct sockaddr*)&conn_addr, &conn_len))) {
        printf("[SS] Connection accepted from %s:%d\n", inet_ntoa(conn_addr.sin_addr), ntohs(conn_addr.sin_port));
        
        pthread_t thread_id;
        connection_t* conn = (connection_t*)malloc(sizeof(connection_t));
        conn->conn_socket = new_conn_sock; // Use new socket

        if (pthread_create(&thread_id, NULL, handle_ss_connection, (void*)conn) < 0) {
            perror("could not create thread");
            free(conn);
        }
        // MODIFIED: Detach thread
        pthread_detach(thread_id);
    }

    if (new_conn_sock < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;
}