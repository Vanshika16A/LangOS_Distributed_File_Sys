#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "CRWD.c" // CRWD.c is modified to include new helper functions
#include "../logger.h"
#include "hash_table.h"


#define MAX_BUFFER_SIZE 1024
#define NAME_SERVER_PORT 8080

void handle_create(int sock, const char* filename, const char* username);
void handle_read(int sock, const char* filename, const char* username);
void handle_write(int sock, const char* filename, int sentence_num, const char* username);
void handle_delete(int sock, const char* filename, const char* username);
void handle_stream(int sock, const char* filename, const char* username);
void handle_undo(int sock, const char* filename, const char* current_user);
void handle_exec(int sock, const char* filename, const char* current_user);
void handle_update_meta(int sock, const char* filename);
void register_user(const char* username, const char* ip_addr);
void register_storage_server(const char* ip, int port, const char* file_list_str);
void handle_list_users(int sock);
void handle_view(int sock, const char* flags, const char* username);
void handle_info(int sock, const char* filename, const char* username);
void handle_add_access(int sock, const char* filename, const char* target_user, const char* perm, const char* current_user);
void handle_rem_access(int sock, const char* filename, const char* target_user, const char* current_user);
void lru_int();

// MODIFIED: This function now handles a persistent session
void* handle_connection(void* client_info_p) {
    client_info_t* client_info = (client_info_t*)client_info_p;
    int sock = client_info->client_socket;
    char* client_ip = inet_ntoa(client_info->client_addr.sin_addr);
    
    char buffer[MAX_BUFFER_SIZE];
    char log_buf[MAX_BUFFER_SIZE + 200]; // Buffer for log messages
    int read_size;
    char current_user[50] = "anonymous"; // User for this session

    // --- 1. Handle Initial Registration ---
    // A persistent session MUST register first.
    if ((read_size = recv(sock, buffer, MAX_BUFFER_SIZE - 1, 0)) > 0) {
        buffer[read_size] = '\0';
        char* command = strtok(buffer, ";\n");

        if (command != NULL && strcmp(command, "REGISTER_CLIENT") == 0) {
            char* username = strtok(NULL, ";\n");
            if (username) {
                register_user(username, client_ip);
                strcpy(current_user, username); // Set user for this session
                snprintf(log_buf, sizeof(log_buf), "Registered client '%s' from IP %s", current_user, client_ip);
                log_message(LOG_INFO, "NameServer", log_buf);
            }
            char response[] = "ACK_CLIENT_REG\n__END__\n";
            send(sock, response, strlen(response), 0);
        } else if (command != NULL && strcmp(command, "REGISTER_SS") == 0) {
             // --- Handle SS Registration ---
            char* ip = strtok(NULL, ";\n");
            char* port_str = strtok(NULL, ";\n");
            // MODIFIED: Now parses the file list string
            char* file_list_str = strtok(NULL, "\n"); // Get rest of the line

            if (ip && port_str) {
                // MODIFIED: Pass file list to registration function
                register_storage_server(ip, atoi(port_str), file_list_str ? file_list_str : "");
            }
            char response[] = "ACK_SS_REG\n__END__\n";
            send(sock, response, strlen(response), 0);
            
            // This is an SS connection, not a client. Close it.
            free(client_info);
            close(sock);
            return NULL;

        } else {
            // Not a valid first command
            printf("[Name Server] Invalid initial command. Closing connection.\n");
            free(client_info);
            close(sock);
            return NULL;
        }
    } else {
        // Client connected and immediately disconnected
        free(client_info);
        close(sock);
        return NULL;
    }

    // --- 2. Handle Command Loop for Registered Client ---
    while ((read_size = recv(sock, buffer, MAX_BUFFER_SIZE - 1, 0)) > 0) {
        buffer[read_size] = '\0';
        char command_line[MAX_BUFFER_SIZE];
        strncpy(command_line, buffer, MAX_BUFFER_SIZE);
        char* command = strtok(buffer, ";\n");

        if (command == NULL) continue;
        
        snprintf(log_buf, sizeof(log_buf), "Request from user '%s' (IP: %s): %s", current_user, client_ip, command);
        log_message(LOG_INFO, "NameServer", log_buf);

        if (strcmp(command, "LIST_USERS") == 0) {
            handle_list_users(sock);
        }
        else if (strcmp(command, "VIEW") == 0) {
            char* flags = strtok(NULL, ";\n");
            handle_view(sock, flags, current_user);
        }
        else if (strcmp(command, "INFO") == 0) {
            char* filename = strtok(NULL, ";\n");
            if (filename)
                handle_info(sock, filename, current_user);
        }
        else if (strcmp(command, "ADDACCESS") == 0) {
            char* filename = strtok(NULL, ";\n");
            char* user = strtok(NULL, ";\n");
            char* perm = strtok(NULL, ";\n");
            if (filename && user && perm) {
            char filename_copy[100];
            char user_copy[50];
            char perm_copy[2];
            strncpy(filename_copy, filename, 99);
            filename_copy[99] = '\0';
            strncpy(user_copy, user, 49);
            user_copy[49] = '\0';
            strncpy(perm_copy, perm, 1);
            perm_copy[1] = '\0';
            handle_add_access(sock, filename_copy, user_copy, perm_copy, current_user);
        }
        }
        else if (strcmp(command, "REMACCESS") == 0) {
            char* filename = strtok(NULL, ";\n");
            char* user = strtok(NULL, ";\n");
            if (filename && user)
                handle_rem_access(sock, filename, user, current_user);
        }
        else if (strcmp(command, "CREATE") == 0) {
            char* filename = strtok(NULL, ";\n");
            if (filename) {
                char filename_copy[100];
                strncpy(filename_copy, filename, 99);
                filename_copy[99] = '\0';
                handle_create(sock, filename_copy, current_user);
            }
        }
        else if (strcmp(command, "READ") == 0) {
            char* filename = strtok(NULL, ";\n");
            if (filename) {
            char filename_copy[100];
            strncpy(filename_copy, filename, 99);
            filename_copy[99] = '\0';
            handle_read(sock, filename_copy, current_user);
        }
        }
        else if (strcmp(command, "WRITE") == 0) {
            char* filename = strtok(NULL, ";\n");
            char* sent_num_str = strtok(NULL, ";\n");
            if (filename && sent_num_str) {
            char filename_copy[100];
            strncpy(filename_copy, filename, 99);
            filename_copy[99] = '\0';
            handle_write(sock, filename_copy, atoi(sent_num_str), current_user);
        }
        }
        else if (strcmp(command, "DELETE") == 0) {
            char* filename = strtok(NULL, ";\n");
            if (filename)
                handle_delete(sock, filename, current_user);
        }
        else if (strcmp(command, "STREAM") == 0) {
            char* filename = strtok(NULL, ";\n");
            if (filename)
                handle_stream(sock, filename, current_user);
        }
        else if (strcmp(command, "UNDO") == 0)
        {
            char* filename = strtok(NULL, ";\n");
            if (filename)
                handle_undo(sock, filename, current_user);
        }
        else if (strcmp(command, "UPDATE_META") == 0)
        {
            char* filename = strtok(NULL, ";\n");
            if(filename)
                handle_update_meta(sock, filename);
        }
        else if (strcmp(command, "EXEC") == 0)
        {
            char* filename = strtok(NULL, ";\n");
            if(filename)
                handle_exec(sock, filename, current_user);
        }
        else {
            char response[] = "ERROR: Unknown command.\n__END__\n";
            send(sock, response, strlen(response), 0);
        }
    }
    
    // Client disconnected
    snprintf(log_buf, sizeof(log_buf), "Client '%s' (IP: %s) disconnected.", current_user, client_ip);
    log_message(LOG_INFO, "NameServer", log_buf);
    
    free(client_info);
    close(sock);
    return NULL;
}

// --- HELPER FUNCTIONS FOR REGISTRATION ---

void register_user(const char* username, const char* ip_addr) {
    pthread_mutex_lock(&data_mutex);

    User* current = user_list_head; 
    while(current) {
        if(strcmp(current->username, username) == 0) {
            strcpy(current->ip_addr, ip_addr);
            printf("[Data] Re-registered user '%s' from IP %s\n", username, ip_addr);
            pthread_mutex_unlock(&data_mutex);
            return;
        }
        current = current->next;
    }

    User* newUser = (User*)malloc(sizeof(User));
    strcpy(newUser->username, username);
    strcpy(newUser->ip_addr, ip_addr);
    newUser->next = user_list_head;
    user_list_head = newUser;

    printf("[Data] Registered user '%s' from IP %s\n", username, ip_addr);
    pthread_mutex_unlock(&data_mutex);
}


// MODIFIED: Signature changed to accept file list
void register_storage_server(const char* ip, int port, const char* file_list_str) {
    pthread_mutex_lock(&data_mutex);
    
    StorageServer* current = ss_list_head;
    while(current) {
        if(strcmp(current->ip_addr, ip) == 0 && current->port == port) {
            printf("[Data] Re-registered SS at %s:%d\n", ip, port);
            pthread_mutex_unlock(&data_mutex);
            return;
        }
        current = current->next;
    }
    
    // Add new SS
    StorageServer* newSS = (StorageServer*)malloc(sizeof(StorageServer));
    strcpy(newSS->ip_addr, ip);
    newSS->port = port;
    newSS->next = ss_list_head;
    ss_list_head = newSS;
    printf("[Data] Registered new SS at %s:%d\n", ip, port);
    
    // MODIFIED: Parse the file list and add metadata
    // This is a simple parser. A robust one would handle the "stream of packets" from Q&A
    if (file_list_str && strlen(file_list_str) > 0) {
        printf("[Data] Registering files from SS: %s\n", file_list_str);
        char* files_copy = strdup(file_list_str);
        char* filename = strtok(files_copy, ",");
        while (filename) {
            if (find_file(filename) == NULL) {
                // File not known, add it. Assume "admin" owner? Or SS owner?
                // For now, just add it with the SS.
                FileMetadata* newFile = (FileMetadata*)malloc(sizeof(FileMetadata));
                strcpy(newFile->filename, filename);
                strcpy(newFile->owner, "ss_owner"); // Placeholder owner
                newFile->word_count = 0; // Unknown
                newFile->char_count = 0; // Unknown
                newFile->last_access = time(NULL);
                newFile->ss = newSS;
                newFile->access_list = NULL; // No access list
                newFile->next = file_list_head;
                file_list_head = newFile;
                ht_insert(file_hash_table, newFile->filename, newFile); // Index in hash table
                printf("[Data] Registered existing file '%s' from SS.\n", filename);
            }
            filename = strtok(NULL, ",");
        }
        free(files_copy);
    }

    pthread_mutex_unlock(&data_mutex);
}


void handle_list_users(int sock) {
    // (No change to this function's logic)
    char response[MAX_BUFFER_SIZE * 2] = ""; 
    
    pthread_mutex_lock(&data_mutex);
    strcat(response, "Registered Users:\n");
    strcat(response, "-----------------\n");
    for (User* current = user_list_head; current != NULL; current = current->next) {
        strcat(response, "-> ");
        strcat(response, current->username);
        strcat(response, "\n");
    }
    pthread_mutex_unlock(&data_mutex);

    strcat(response, "__END__\n");
    send(sock, response, strlen(response), 0);
}

void handle_view(int sock, const char* flags, const char* username) {
    char response[MAX_BUFFER_SIZE * 4] = ""; 
    
    // MODIFIED: Filled in the TODOs
    int show_details = (flags && (strstr(flags, "l") != NULL));
    int show_all = (flags && (strstr(flags, "a") != NULL));

    pthread_mutex_lock(&data_mutex);

    if (show_details) {
            snprintf(response, sizeof(response), "| %-20s | %-12s | %-17s |\n", "Filename", "Owner", "Location (SS)");
            strcat(response, "---------------------------------------------------------\n");
    }

    int file_count = 0;
    for (FileMetadata* current = file_list_head; current != NULL; current = current->next) {
        
        // MODIFIED: This is the permission check
        // If show_all is false AND the user does NOT have permission, skip this file.
        if (!show_all && !check_permission(current, username, 'R')) {
            continue; 
        }
        
        // If we are here, we have permission (or show_all is true)
        file_count++;
        char line[256];
        if (show_details) {
            char loc[40] = "N/A";
            if (current->ss) {
                    snprintf(loc, 40, "%s:%d", current->ss->ip_addr, current->ss->port);
            }
            snprintf(line, sizeof(line), "| %-20s | %-12s | %-17s |\n", 
                    current->filename, current->owner, loc);
        } else {
            snprintf(line, sizeof(line), "%s\n", current->filename);
        }
        
        // Prevent buffer overflow
        if (strlen(response) + strlen(line) < sizeof(response) - 100) {
            strcat(response, line);
        }
    }

    // Add a footer if no files were found to show
    if (file_count == 0) {
        if (show_all) {
            strcat(response, "No files found in the system.\n");
        } else {
            strcat(response, "No files accessible to you.\n");
        }
    }

    pthread_mutex_unlock(&data_mutex);
    
    strcat(response, "__END__\n");
    send(sock, response, strlen(response), 0);
}


int main() {
    // (No change to this function's logic)
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    pthread_mutex_init(&data_mutex, NULL);
    file_hash_table = ht_create();
    lru_init();
    load_metadata();
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Could not create socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(NAME_SERVER_PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    printf("[Name Server] Bind successful on port %d.\n", NAME_SERVER_PORT);

    listen(server_sock, 10);
    printf("[Name Server] Waiting for incoming connections...\n");

    while ((client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len))) {
        char log_buf[100];
        snprintf(log_buf, sizeof(log_buf), "Connection accepted from %s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        log_message(LOG_INFO, "NameServer", log_buf);
        
        pthread_t thread_id;
        client_info_t* client_info = malloc(sizeof(client_info_t));
        client_info->client_socket = client_sock;
        client_info->client_addr = client_addr;

        if (pthread_create(&thread_id, NULL, handle_connection, (void*)client_info) < 0) {
            perror("could not create thread");
            return 1;
        }
        
        pthread_detach(thread_id);
    }

    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }
    
    pthread_mutex_destroy(&data_mutex);

    return 0;
}