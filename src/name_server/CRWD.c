/*
 * CRWD.c
 *
 * This file contains the Name Server handlers for
 * CREATE, READ, WRITE, and DELETE.
 * It is #include'd by name_server.c.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
// ADDED: New includes for NS-to-SS communication
#include <unistd.h>
#include <arpa/inet.h> 

#define MAX_BUFFER_SIZE 1024
#define SS_RESPONSE_LEN 4096 // For reading SS ACKs

// Struct definitions (no change)
typedef struct StorageServer {
    char ip_addr[20];
    int port;
    struct StorageServer* next;
} StorageServer;

typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} client_info_t;

typedef struct AccessNode {
    char username[50];
    char permission; // 'R' for read, 'W' for write
    struct AccessNode* next;
} AccessNode;

typedef struct FileMetadata {
    char filename[100];
    char owner[50];
    int word_count;
    int char_count;
    time_t last_access;
    StorageServer* ss; 
    AccessNode* access_list;
    struct FileMetadata* next;
} FileMetadata;

typedef struct User {
    char username[50];
    char ip_addr[20];
    struct User* next;
} User;


FileMetadata* file_list_head = NULL; 
StorageServer* ss_list_head = NULL;
User* user_list_head = NULL;
pthread_mutex_t data_mutex;

// Helper to find a file (no change)
FileMetadata* find_file(const char* filename) {
    FileMetadata* current = file_list_head;
    while (current) {
        if (strcmp(current->filename, filename) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// 'R' = Read, 'W' = Write (no change)
int check_permission(FileMetadata* file, const char* username, char perm) {
    if (strcmp(file->owner, username) == 0) {
        return 1; // Owner has all permissions
    }
    AccessNode* current = file->access_list;
    while (current) {
        if (strcmp(current->username, username) == 0) {
            if (current->permission == 'W' || current->permission == perm) {
                return 1; 
            }
        }
        current = current->next;
    }
    return 0; // No permission
}

// +++ ADDED: Helper function for NS to command SS +++
// This is used for the NM-mediated CREATE and DELETE flows.
// Returns 1 on success, 0 on failure. Fills response_buffer.
int connect_and_send_to_ss(const char* ip, int port, const char* command, char* response_buffer) {
    int sock;
    struct sockaddr_in ss_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("[NS] Could not create socket to SS");
        snprintf(response_buffer, SS_RESPONSE_LEN, "ERROR: NS could not create socket to SS");
        return 0;
    }

    ss_addr.sin_addr.s_addr = inet_addr(ip);
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&ss_addr, sizeof(ss_addr)) < 0) { 
        perror("[NS] SS Connect failed");
        snprintf(response_buffer, SS_RESPONSE_LEN, "ERROR: NS could not connect to SS");
        close(sock);
        return 0;
    }

    // Send command to SS
    if (send(sock, command, strlen(command), 0) < 0) {
        perror("[NS] Send to SS failed");
        snprintf(response_buffer, SS_RESPONSE_LEN, "ERROR: NS could not send to SS");
        close(sock);
        return 0;
    }

    // Read loop to get the full response until __SS_END__
    int total_read = 0;
    int read_size;
    response_buffer[0] = '\0';

    while ((read_size = recv(sock, response_buffer + total_read, SS_RESPONSE_LEN - total_read - 1, 0)) > 0) {
        total_read += read_size;
        response_buffer[total_read] = '\0';
        if (strstr(response_buffer, "__SS_END__")) {
            break;
        }
    }

    if (read_size <= 0) {
        perror("[NS] Recv from SS failed");
        snprintf(response_buffer, SS_RESPONSE_LEN, "ERROR: NS did not receive reply from SS");
        close(sock);
        return 0;
    }

    // Clean up the __SS_END__ token
    char* end_token = strstr(response_buffer, "__SS_END__");
    if (end_token) {
        *end_token = '\0';
    }

    close(sock);
    return 1;
}
User* find_user(const char* username) {
    User* current = user_list_head;
    while (current) {
        if (strcmp(current->username, username) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}
// Add this entire function to CRWD.c, near the other "handle_" functions

void handle_info(int sock, const char* filename, const char* username) {
    char response[MAX_BUFFER_SIZE * 2] = ""; // Increased buffer size
    int len = 0;

    pthread_mutex_lock(&data_mutex);

    FileMetadata* file = find_file(filename);

    // 1. Check if file exists
    if (!file) {
        snprintf(response, sizeof(response), "ERROR: File '%s' not found.\n", filename);
        pthread_mutex_unlock(&data_mutex);
        strcat(response, "__END__\n");
        send(sock, response, strlen(response), 0);
        return;
    }

    // 2. Check for read permission
    if (!check_permission(file, username, 'R')) {
        snprintf(response, sizeof(response), "ERROR: Permission denied for file '%s'.\n", filename);
        pthread_mutex_unlock(&data_mutex);
        strcat(response, "__END__\n");
        send(sock, response, strlen(response), 0);
        return;
    }

    // 3. File exists and user has permission, build the response
    
    // Format the time
    char time_buf[100];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&file->last_access));

    // Add file details
    len += snprintf(response + len, sizeof(response) - len, "File: %s\n", file->filename);
    len += snprintf(response + len, sizeof(response) - len, "Owner: %s\n", file->owner);
    len += snprintf(response + len, sizeof(response) - len, "Last Modified: %s\n", time_buf);
    len += snprintf(response + len, sizeof(response) - len, "Word Count: %d\n", file->word_count);
    len += snprintf(response + len, sizeof(response) - len, "Char Count: %d\n", file->char_count);

    // Add access list
    len += snprintf(response + len, sizeof(response) - len, "Access: ");
    len += snprintf(response + len, sizeof(response) - len, "%s (RW)", file->owner); // Owner

    AccessNode* current = file->access_list;
    while (current) {
        if (len < sizeof(response) - 100) {
            len += snprintf(response + len, sizeof(response) - len, ", %s (%c)", current->username, current->permission);
        }
        current = current->next;
    }
    len += snprintf(response + len, sizeof(response) - len, "\n");

    pthread_mutex_unlock(&data_mutex);

    // 4. Send the final response
    strncat(response, "__END__\n", sizeof(response) - strlen(response) - 1);
    send(sock, response, strlen(response), 0);
}
void handle_add_access(int sock, const char* filename, const char* target_user, const char* perm, const char* current_user)
{
    char response[MAX_BUFFER_SIZE];
    int access_updated = 0; // Flag to see if we updated an existing node

    pthread_mutex_lock(&data_mutex);

    FileMetadata* file = find_file(filename);

    // 1. Check 1: Does the file exist?
    if (!file) {
        snprintf(response, sizeof(response), "ERROR: File '%s' not found.\n__END__\n", filename);
        pthread_mutex_unlock(&data_mutex);
        send(sock, response, strlen(response), 0);
        return;
    }

    // 2. Check 2: Is the current user the owner?
    if (strcmp(file->owner, current_user) != 0) {
        snprintf(response, sizeof(response), "ERROR: Only the file owner ('%s') can change permissions.\n__END__\n", file->owner);
        pthread_mutex_unlock(&data_mutex);
        send(sock, response, strlen(response), 0);
        return;
    }

    // 3. Check 3: Does the target user exist in the system? (Per Q&A)
    if (find_user(target_user) == NULL) {
        snprintf(response, sizeof(response), "ERROR: User '%s' is not registered in the system.\n__END__\n", target_user);
        pthread_mutex_unlock(&data_mutex);
        send(sock, response, strlen(response), 0);
        return;
    }

    // 4. Check 4: Is the permission flag valid?
    if (perm[0] != 'R' && perm[0] != 'W') {
         snprintf(response, sizeof(response), "ERROR: Invalid permission '%s'. Must be 'R' or 'W'.\n__END__\n", perm);
        pthread_mutex_unlock(&data_mutex);
        send(sock, response, strlen(response), 0);
        return;
    }
    
    // 5. Logic: Find and update, or create new access node
    AccessNode* current = file->access_list;
    while(current) {
        if (strcmp(current->username, target_user) == 0) {
            // Found the user! Just update their permission.
            current->permission = perm[0];
            access_updated = 1;
            break;
        }
        current = current->next;
    }

    if (!access_updated) {
        // User was not in the list, create a new node
        AccessNode* new_node = (AccessNode*)malloc(sizeof(AccessNode));
        if (!new_node) {
             snprintf(response, sizeof(response), "ERROR: Name Server out of memory.\n__END__\n");
             pthread_mutex_unlock(&data_mutex);
             send(sock, response, strlen(response), 0);
             return;
        }
        strcpy(new_node->username, target_user);
        new_node->permission = perm[0];
        
        // Add to the head of the list
        new_node->next = file->access_list;
        file->access_list = new_node;
    }

    // 6. Send success response
    snprintf(response, sizeof(response), "Access for '%s' on '%s' set to '%c'.\n__END__\n", target_user, filename, perm[0]);
    pthread_mutex_unlock(&data_mutex);
    send(sock, response, strlen(response), 0);
}


// --- COMPLETED FUNCTION ---
void handle_rem_access(int sock, const char* filename, const char* target_user, const char* current_user)
{
    char response[MAX_BUFFER_SIZE];
    int node_found = 0;

    pthread_mutex_lock(&data_mutex);

    FileMetadata* file = find_file(filename);

    // 1. Check 1: Does the file exist?
    if (!file) {
        snprintf(response, sizeof(response), "ERROR: File '%s' not found.\n__END__\n", filename);
        pthread_mutex_unlock(&data_mutex);
        send(sock, response, strlen(response), 0);
        return;
    }

    // 2. Check 2: Is the current user the owner?
    if (strcmp(file->owner, current_user) != 0) {
        snprintf(response, sizeof(response), "ERROR: Only the file owner ('%s') can change permissions.\n__END__\n", file->owner);
        pthread_mutex_unlock(&data_mutex);
        send(sock, response, strlen(response), 0);
        return;
    }

    // 3. Logic: Find and remove the node
    AccessNode* current = file->access_list;
    AccessNode* prev = NULL;

    while (current != NULL) {
        if (strcmp(current->username, target_user) == 0) {
            // Found the node to remove
            node_found = 1;
            if (prev == NULL) {
                // It's the head of the list
                file->access_list = current->next;
            } else {
                // It's in the middle or at the end
                prev->next = current->next;
            }
            free(current);
            break;
        }
        // Move to the next node
        prev = current;
        current = current->next;
    }

    // 4. Send response
    if (node_found) {
        snprintf(response, sizeof(response), "Access for '%s' on '%s' has been removed.\n__END__\n", target_user, filename);
    } else {
        snprintf(response, sizeof(response), "INFO: User '%s' had no special access on '%s' to remove.\n__END__\n", target_user, filename);
    }
    
    pthread_mutex_unlock(&data_mutex);
    send(sock, response, strlen(response), 0);
}

// MODIFIED: Complete rewrite to be NM-mediated
void handle_create(int sock, const char* filename, const char* username) {
    char response[MAX_BUFFER_SIZE];
    char ss_command[MAX_BUFFER_SIZE];
    char ss_response[SS_RESPONSE_LEN];

    pthread_mutex_lock(&data_mutex);

    if (find_file(filename)) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: File '%s' already exists.\n__END__\n", filename);
        send(sock, response, strlen(response), 0);
        return;
    }
    
    if (!ss_list_head) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: No Storage Servers available.\n__END__\n");
        send(sock, response, strlen(response), 0);
        return;
    }

    StorageServer* target_ss = ss_list_head; // Simple load balancing: just pick the first
    
    // --- 1. Add metadata to NS first ---
    FileMetadata* newFile = (FileMetadata*)malloc(sizeof(FileMetadata));
    strcpy(newFile->filename, filename);
    strcpy(newFile->owner, username);
    newFile->word_count = 0;
    newFile->char_count = 0;
    newFile->last_access = time(NULL);
    newFile->ss = target_ss;
    
    AccessNode* ownerAccess = (AccessNode*)malloc(sizeof(AccessNode));
    strcpy(ownerAccess->username, username);
    ownerAccess->permission = 'W';
    ownerAccess->next = NULL;
    newFile->access_list = ownerAccess;

    newFile->next = file_list_head;
    file_list_head = newFile;
    
    // We are done with global lists, unlock
    pthread_mutex_unlock(&data_mutex);

    // --- 2. Forward request to SS ---
    printf("[NS] Forwarding CREATE request to SS at %s:%d\n", target_ss->ip_addr, target_ss->port);
    snprintf(ss_command, sizeof(ss_command), "SS_CREATE;%s\n", filename);
    
    if (connect_and_send_to_ss(target_ss->ip_addr, target_ss->port, ss_command, ss_response)) {
        // SS responded
        if (strstr(ss_response, "ACK_CREATE")) {
            snprintf(response, sizeof(response), "File '%s' created successfully.\n__END__\n", filename);
        } else {
            // SS failed. TODO: Roll back metadata creation?
            printf("[NS] SS Error for CREATE: %s\n", ss_response);
            snprintf(response, sizeof(response), "ERROR: Storage Server failed: %.500s\n__END__\n", ss_response);
            // For now, we leave the "zombie" metadata.
        }
    } else {
        // NS-SS connection failed. TODO: Roll back.
        printf("[NS] Failed to contact SS for CREATE.\n");
        snprintf(response, sizeof(response), "ERROR: Name Server could not contact Storage Server.\n__END__\n");
    }

    // --- 3. Send final ACK to client ---
    send(sock, response, strlen(response), 0);
}

// MODIFIED: Added permission check
void handle_read(int sock, const char* filename, const char* username) {
    char response[MAX_BUFFER_SIZE];
    pthread_mutex_lock(&data_mutex);
    
    FileMetadata* file = find_file(filename);
    
    if (!file) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: File '%s' not found.\n__END__\n", filename);
        send(sock, response, strlen(response), 0);
        return;
    }

    // +++ ADDED: Permission Check +++
    if (!check_permission(file, username, 'R')) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: Permission denied for file '%s'.\n__END__\n", filename);
        send(sock, response, strlen(response), 0);
        return;
    }
    // +++ END ADDED +++

    StorageServer* target_ss = file->ss;
    pthread_mutex_unlock(&data_mutex);

    printf("[NS] Redirecting client '%s' to SS at %s:%d for READ\n", username, target_ss->ip_addr, target_ss->port);
    snprintf(response, sizeof(response), "REDIRECT_READ;%s;%d;%s\n__END__\n",
            target_ss->ip_addr, target_ss->port, filename);
    send(sock, response, strlen(response), 0);
}

// MODIFIED: Added permission check
void handle_write(int sock, const char* filename, int sentence_num, const char* username) {
    char response[MAX_BUFFER_SIZE];
    pthread_mutex_lock(&data_mutex);
    
    FileMetadata* file = find_file(filename);
    
    if (!file) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: File '%s' not found.\n__END__\n", filename);
        send(sock, response, strlen(response), 0);
        return;
    }
    
    // +++ ADDED: Permission Check (must have 'W' to write) +++
    if (!check_permission(file, username, 'W')) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: Write permission denied for file '%s'.\n__END__\n", filename);
        send(sock, response, strlen(response), 0);
        return;
    }
    // +++ END ADDED +++

    StorageServer* target_ss = file->ss;
    pthread_mutex_unlock(&data_mutex);
    
    printf("[NS] Redirecting client '%s' to SS at %s:%d for WRITE\n", username, target_ss->ip_addr, target_ss->port);
    snprintf(response, sizeof(response), "REDIRECT_WRITE;%s;%d;%s;%d\n__END__\n",
            target_ss->ip_addr, target_ss->port, filename, sentence_num);
    send(sock, response, strlen(response), 0);
}

// MODIFIED: Complete rewrite to be NM-mediated and atomic
void handle_delete(int sock, const char* filename, const char* username) {
    char response[MAX_BUFFER_SIZE];
    char ss_command[MAX_BUFFER_SIZE];
    char ss_response[SS_RESPONSE_LEN];

    pthread_mutex_lock(&data_mutex);
    
    FileMetadata* file = find_file(filename);
    
    if (!file) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: File '%s' not found.\n__END__\n", filename);
        send(sock, response, strlen(response), 0);
        return;
    }

    if (strcmp(file->owner, username) != 0) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: Only the owner can delete file '%s'.\n__END__\n", filename);
        send(sock, response, strlen(response), 0);
        return;
    }

    StorageServer* target_ss = file->ss;
    
    // --- 1. Forward request to SS WHILE STILL HOLDING LOCK ---
    printf("[NS] Forwarding DELETE request to SS at %s:%d\n", target_ss->ip_addr, target_ss->port);
    snprintf(ss_command, sizeof(ss_command), "SS_DELETE;%s\n", filename);

    if (connect_and_send_to_ss(target_ss->ip_addr, target_ss->port, ss_command, ss_response)) {
        // SS responded
        if (strstr(ss_response, "ACK_DELETE")) {
            // --- 2. SS succeeded, now delete metadata ---
            // This is the logic from old handle_delete_metadata
            FileMetadata* prev = NULL;
            FileMetadata* current = file_list_head;
            while(current) {
                if (strcmp(current->filename, filename) == 0) {
                    if (prev) {
                        prev->next = current->next;
                    } else {
                        file_list_head = current->next;
                    }
                    
                    AccessNode* access = current->access_list;
                    while (access) {
                        AccessNode* temp = access;
                        access = access->next;
                        free(temp);
                    }
                    free(current);
                    printf("[NS] Deleted metadata for '%s'\n", filename);
                    break;
                }
                prev = current;
                current = current->next;
            }
            snprintf(response, sizeof(response), "File '%s' successfully deleted from system.\n__END__\n", filename);

        } else {
            // SS failed to delete, so we don't touch metadata
            printf("[NS] SS Error for DELETE: %s\n", ss_response);
            snprintf(response, sizeof(response), "ERROR: Storage Server failed: %.500s\n__END__\n", ss_response);
        }
    } else {
        // NS-SS connection failed. Do not delete metadata.
        printf("[NS] Failed to contact SS for DELETE.\n");
        snprintf(response, sizeof(response), "ERROR: Name Server could not contact Storage Server.\n__END__\n");
    }
    
    // --- 3. Unlock mutex and send final response to client ---
    pthread_mutex_unlock(&data_mutex);
    send(sock, response, strlen(response), 0);
}

void handle_stream(int sock, const char* filename, const char* username) {
    char response[MAX_BUFFER_SIZE];
    pthread_mutex_lock(&data_mutex);
    
    FileMetadata* file = find_file(filename);
    
    if (!file) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: File '%s' not found.\n__END__\n", filename);
        send(sock, response, strlen(response), 0);
        return;
    }

    if (!check_permission(file, username, 'R')) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: Permission denied for file '%s'.\n__END__\n", filename);
        send(sock, response, strlen(response), 0);
        return;
    }

    StorageServer* target_ss = file->ss;
    pthread_mutex_unlock(&data_mutex);

    printf("[NS] Redirecting client '%s' to SS at %s:%d for STREAM\n", username, target_ss->ip_addr, target_ss->port);
    snprintf(response, sizeof(response), "REDIRECT_STREAM;%s;%d;%s\n__END__\n",
            target_ss->ip_addr, target_ss->port, filename);
    send(sock, response, strlen(response), 0);
}

// This assumes you have connect_and_send_to_ss in this file
// and that SS_RESPONSE_LEN is defined (e.g., #define SS_RESPONSE_LEN 4096)

void handle_undo(int sock, const char* filename, const char* current_user)
{
    char response[MAX_BUFFER_SIZE];
    char ss_command[MAX_BUFFER_SIZE];
    char ss_response[SS_RESPONSE_LEN];

    pthread_mutex_lock(&data_mutex);

    FileMetadata* file = find_file(filename);

    if (!file) {
        snprintf(response, sizeof(response), "ERROR: File '%s' not found.\n__END__\n", filename);
        pthread_mutex_unlock(&data_mutex);
        send(sock, response, strlen(response), 0);
        return;
    }

    // 1. Check Permission (as per Q&A)
    //    (Fixing bug: must pass 'file' object, not 'filename' string)
    if (!check_permission(file, current_user, 'W')) {
        snprintf(response, sizeof(response), "ERROR: Write permission required to undo '%s'.\n__END__\n", filename);
        pthread_mutex_unlock(&data_mutex);
        send(sock, response, strlen(response), 0);
        return;
    }

    StorageServer* target_ss = file->ss;
    
    // We are done with metadata, unlock
    pthread_mutex_unlock(&data_mutex);

    // 2. Forward request to SS (NM-mediated)
    printf("[NS] Forwarding UNDO request to SS at %s:%d\n", target_ss->ip_addr, target_ss->port);
    snprintf(ss_command, sizeof(ss_command), "SS_UNDO;%s\n", filename);

    if (connect_and_send_to_ss(target_ss->ip_addr, target_ss->port, ss_command, ss_response)) {
        // SS responded
        if (strstr(ss_response, "ACK_UNDO")) {
            snprintf(response, sizeof(response), "Undo successful for '%s'.\n__END__\n", filename);
        } else {
            // SS failed (e.g., no .bak file)
            snprintf(response, sizeof(response), "ERROR: Undo failed on Storage Server: %.500s\n__END__\n", ss_response);
        }
    } else {
        // NS-SS connection failed
        snprintf(response, sizeof(response), "ERROR: Name Server could not contact Storage Server for undo.\n__END__\n");
    }

    // 3. Send final ACK to client
    send(sock, response, strlen(response), 0);
}
// Delete your old calc_words and calc_chars functions.
// Use this corrected handle_update_meta function instead.

void handle_update_meta(int sock, const char* filename)
{
    char ss_command[MAX_BUFFER_SIZE];
    char file_content[SS_RESPONSE_LEN]; // Buffer to hold the file
    StorageServer* target_ss;
    FileMetadata* file;

    // --- 1. Find file and update time ---
    pthread_mutex_lock(&data_mutex);
    file = find_file(filename);
    if (!file) {
        pthread_mutex_unlock(&data_mutex);
        // No need to send error, client doesn't wait for one
        return;
    }
    file->last_access = time(NULL);
    target_ss = file->ss; // Get SS info
    pthread_mutex_unlock(&data_mutex);

    // --- 2. Fetch file content from SS (outside the lock) ---
    snprintf(ss_command, sizeof(ss_command), "SS_READ;%s\n", filename);
    if (!connect_and_send_to_ss(target_ss->ip_addr, target_ss->port, ss_command, file_content)) {
        printf("[NS] UPDATE_META: Failed to fetch file %s from SS.\n", filename);
        return;
    }

    // --- 3. Calculate word and char count ---
    // Note: strlen is the correct char count. (Your 'strlen - 1' was a bug)
    int char_count = strlen(file_content);
    int word_count = 0;
    
    // We must strdup because strtok modifies the string
    char* content_copy = strdup(file_content);
    if (!content_copy) return; // Out of memory

    char* word = strtok(content_copy, " \t\n\r");
    while (word != NULL) {
        word_count++;
        word = strtok(NULL, " \t\n\r");
    }
    free(content_copy); // Clean up the copy

    // --- 4. Re-lock and update the metadata struct ---
    pthread_mutex_lock(&data_mutex);
    file = find_file(filename); // Find file again, it might have been deleted
    if (file) {
        file->word_count = word_count;
        file->char_count = char_count;
        printf("[NS] Updated metadata for %s: %d words, %d chars\n", filename, word_count, char_count);
    }
    pthread_mutex_unlock(&data_mutex);
    char response[] = "ACK_META_UPDATE\n__END__\n";
    send(sock, response, strlen(response), 0);
}

#include <sys/wait.h> // Make sure this is included at the top of CRWD.c

void handle_exec(int sock, const char* filename, const char* current_user)
{
    char response[MAX_BUFFER_SIZE * 4]; // Buffer for the command's output
    char ss_command[MAX_BUFFER_SIZE];
    char file_content[SS_RESPONSE_LEN];
    StorageServer* target_ss;
    char ss_ip[20];
    int ss_port;

    pthread_mutex_lock(&data_mutex);

    FileMetadata* file = find_file(filename);

    // 1. Check permissions
    if (!file) {
        snprintf(response, sizeof(response), "ERROR: File '%s' not found.\n__END__\n", filename);
        pthread_mutex_unlock(&data_mutex);
        send(sock, response, strlen(response), 0);
        return;
    }

    if (!check_permission(file, current_user, 'R')) {
        snprintf(response, sizeof(response), "ERROR: Read permission denied for file '%s'.\n__END__\n", filename);
        pthread_mutex_unlock(&data_mutex);
        send(sock, response, strlen(response), 0);
        return;
    }

    // Copy SS info so we can unlock the mutex
    target_ss = file->ss;
    strcpy(ss_ip, target_ss->ip_addr);
    ss_port = target_ss->port;
    
    pthread_mutex_unlock(&data_mutex);

    // 2. NS acts as a client to get the file from SS
    snprintf(ss_command, sizeof(ss_command), "SS_READ;%s\n", filename);
    if (!connect_and_send_to_ss(ss_ip, ss_port, ss_command, file_content)) {
        snprintf(response, sizeof(response), "ERROR: NS failed to fetch file from SS.\n__END__\n");
        send(sock, response, strlen(response), 0);
        return;
    }

    // 3. Save file content to a temporary script
    char tmp_filename[] = "/tmp/docs_exec.XXXXXX";
    int tmp_fd = mkstemp(tmp_filename);
    if (tmp_fd == -1) {
        snprintf(response, sizeof(response), "ERROR: NS failed to create temp file for execution.\n__END__\n");
        send(sock, response, strlen(response), 0);
        return;
    }
    write(tmp_fd, file_content, strlen(file_content));
    close(tmp_fd);

    // 4. Fork, execute, and capture output
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        snprintf(response, sizeof(response), "ERROR: NS failed to create pipe.\n__END__\n");
        send(sock, response, strlen(response), 0);
        remove(tmp_filename);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        snprintf(response, sizeof(response), "ERROR: NS failed to fork.\n__END__\n");
        send(sock, response, strlen(response), 0);
        remove(tmp_filename);
        return;
    }

    if (pid == 0) { // --- Child Process ---
        close(pipe_fd[0]); // Close read end of pipe
        dup2(pipe_fd[1], STDOUT_FILENO); // Redirect stdout to pipe
        dup2(pipe_fd[1], STDERR_FILENO); // Redirect stderr to pipe
        close(pipe_fd[1]);

        // Execute the script
        execlp("bash", "bash", tmp_filename, NULL);
        
        // If execlp fails
        perror("execlp failed");
        exit(1);

    } else { // --- Parent Process ---
        close(pipe_fd[1]); // Close write end of pipe

        char output_buffer[MAX_BUFFER_SIZE * 4];
        int total_read = 0;
        int read_size;

        // Read all output from the child process
        while ((read_size = read(pipe_fd[0], output_buffer + total_read, (sizeof(output_buffer) - total_read - 1))) > 0) {
            total_read += read_size;
        }
        output_buffer[total_read] = '\0';
        
        wait(NULL); // Wait for the child to terminate
        close(pipe_fd[0]);
        remove(tmp_filename); // Clean up the temp file

        // 5. Send the captured output back to the client
        strncat(output_buffer, "\n__END__\n", sizeof(output_buffer) - strlen(output_buffer) - 1);
        send(sock, output_buffer, strlen(output_buffer), 0);
    }
}