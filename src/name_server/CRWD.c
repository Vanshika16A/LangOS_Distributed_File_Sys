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
#define MAX_BUFFER_SIZE 1024

// Struct definitions
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

// connected user
typedef struct User {
    char username[50];
    char ip_addr[20];
    struct User* next;
} User;


FileMetadata* file_list_head = NULL; 
StorageServer* ss_list_head = NULL; //head of linked list for all storage servers
User* user_list_head = NULL; //head of linked list for all users
pthread_mutex_t data_mutex; // Mutex to protect our global lists
// Helper to find a file
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


// 'R' = Read, 'W' = Write
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


void handle_create(int sock, const char* filename, const char* username) {
    char response[MAX_BUFFER_SIZE];
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

    // picking the first storage server to store the created file
    StorageServer* target_ss = ss_list_head; 
    
    
    FileMetadata* newFile = (FileMetadata*)malloc(sizeof(FileMetadata));
    strcpy(newFile->filename, filename);
    strcpy(newFile->owner, username);
    newFile->word_count = 0;
    newFile->char_count = 0;
    newFile->last_access = time(NULL);
    newFile->ss = target_ss; // linked to the chosen sotrage server
    
    // Create access node for the owner (W permission)
    AccessNode* ownerAccess = (AccessNode*)malloc(sizeof(AccessNode));
    strcpy(ownerAccess->username, username);
    ownerAccess->permission = 'W';
    ownerAccess->next = NULL;
    newFile->access_list = ownerAccess;

    // Add to file list
    newFile->next = file_list_head;
    file_list_head = newFile;

    pthread_mutex_unlock(&data_mutex);

    // Send REDIRECT command to client
    printf("[NS] Redirecting client '%s' to SS at %s:%d for CREATE\n", username, target_ss->ip_addr, target_ss->port);
    snprintf(response, sizeof(response), "REDIRECT_CREATE;%s;%d;%s\n__END__\n", 
             target_ss->ip_addr, target_ss->port, filename);
    send(sock, response, strlen(response), 0);
}

// READ
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

    StorageServer* target_ss = file->ss;
    pthread_mutex_unlock(&data_mutex);

    printf("[NS] Redirecting client '%s' to SS at %s:%d for READ\n", username, target_ss->ip_addr, target_ss->port);
    snprintf(response, sizeof(response), "REDIRECT_READ;%s;%d;%s\n__END__\n",
             target_ss->ip_addr, target_ss->port, filename);
    send(sock, response, strlen(response), 0);
}

// WRITE
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
    

    StorageServer* target_ss = file->ss;
    pthread_mutex_unlock(&data_mutex);
    
    printf("[NS] Redirecting client '%s' to SS at %s:%d for WRITE\n", username, target_ss->ip_addr, target_ss->port);
    snprintf(response, sizeof(response), "REDIRECT_WRITE;%s;%d;%s;%d\n__END__\n",
             target_ss->ip_addr, target_ss->port, filename, sentence_num);
    send(sock, response, strlen(response), 0);
}

// DELETE: Client wants to delete a file
void handle_delete(int sock, const char* filename, const char* username) {
    char response[MAX_BUFFER_SIZE];
    pthread_mutex_lock(&data_mutex);
    
    FileMetadata* file = find_file(filename);
    
    if (!file) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: File '%s' not found.\n__END__\n", filename);
        send(sock, response, strlen(response), 0);
        return;
    }

    // Only owner can delete
    if (strcmp(file->owner, username) != 0) {
        pthread_mutex_unlock(&data_mutex);
        snprintf(response, sizeof(response), "ERROR: Only the owner can delete file '%s'.\n__END__\n", filename);
        send(sock, response, strlen(response), 0);
        return;
    }

    StorageServer* target_ss = file->ss;
    pthread_mutex_unlock(&data_mutex);
    
    printf("[NS] Redirecting client '%s' to SS at %s:%d for DELETE\n", username, target_ss->ip_addr, target_ss->port);
    snprintf(response, sizeof(response), "REDIRECT_DELETE;%s;%d;%s\n__END__\n",
             target_ss->ip_addr, target_ss->port, filename);
    send(sock, response, strlen(response), 0);
}

// DELETE_METADATA: Client confirms SS deleted the file
void handle_delete_metadata(int sock, const char* filename, const char* username) {
    char response[MAX_BUFFER_SIZE];
    pthread_mutex_lock(&data_mutex);
    
    FileMetadata* prev = NULL;
    FileMetadata* current = file_list_head;
    
    while(current) {
        if (strcmp(current->filename, filename) == 0) {
            // Found it. Check owner again just in case.
            if (strcmp(current->owner, username) != 0) {
                snprintf(response, sizeof(response), "ERROR: Permission denied for metadata deletion.\n__END__\n");
                break;
            }
            
            // Unlink from list
            if (prev) {
                prev->next = current->next;
            } else {
                file_list_head = current->next;
            }
            
            // Free access list
            AccessNode* access = current->access_list;
            while (access) {
                AccessNode* temp = access;
                access = access->next;
                free(temp);
            }
            
            // Free file node
            free(current);
            printf("[NS] Deleted metadata for '%s'\n", filename);
            snprintf(response, sizeof(response), "File '%s' successfully deleted from system.\n__END__\n", filename);
            break;
        }
        prev = current;
        current = current->next;
    }
    
    if (!current) { // Loop finished without finding
        snprintf(response, sizeof(response), "ERROR: Metadata for '%s' not found.\n__END__\n", filename);
    }
    
    pthread_mutex_unlock(&data_mutex);
    send(sock, response, strlen(response), 0);
}