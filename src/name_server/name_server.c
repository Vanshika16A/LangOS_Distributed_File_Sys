#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "CRWD.c"

#define MAX_BUFFER_SIZE 1024
#define NAME_SERVER_PORT 8080


void handle_create(int sock, const char* filename, const char* username);
void handle_read(int sock, const char* filename, const char* username);
void handle_write(int sock, const char* filename, int sentence_num, const char* username);
void handle_delete(int sock, const char* filename, const char* username);
void handle_delete_metadata(int sock, const char* filename, const char* username);


void register_user(const char* username, const char* ip_addr);
void register_storage_server(const char* ip, int port);
void handle_list_users(int sock);
void handle_view(int sock, const char* flags, const char* username);
void handle_info(int sock, const char* filename);
void handle_add_access(int sock, const char* filename, const char* user, const char* perm);
void handle_rem_access(int sock, const char* filename, const char* user);


void* handle_connection(void* client_info_p) {
    client_info_t* client_info = (client_info_t*)client_info_p;
    int sock = client_info->client_socket;
    char* client_ip = inet_ntoa(client_info->client_addr.sin_addr);
    
    char buffer[MAX_BUFFER_SIZE];
    int read_size;
    char current_user[50] = "anonymous"; // Is it needed?
    if ((read_size = recv(sock, buffer, MAX_BUFFER_SIZE - 1, 0)) > 0) {
        buffer[read_size] = '\0';
        char* command = strtok(buffer, ";\n");

        if (command != NULL) {
            printf("[Name Server] Received Command: %s\n", command);

            if (strcmp(command, "REGISTER_SS") == 0) {
                char* ip = strtok(NULL, ";\n");
                char* port_str = strtok(NULL, ";\n");
                if (ip && port_str) {
                    register_storage_server(ip, atoi(port_str));
                }
                char response[] = "ACK_SS_REG\n__END__\n";
                send(sock, response, strlen(response), 0);
            } 
            else if (strcmp(command, "REGISTER_CLIENT") == 0) {
                char* username = strtok(NULL, ";\n");
                if (username) {
                    register_user(username, client_ip);
                    strcpy(current_user, username); // Set user for this session
                }
                char response[] = "ACK_CLIENT_REG\n__END__\n";
                send(sock, response, strlen(response), 0);
            }
            if (strcmp(current_user, "anonymous") == 0) { //additional funcionality. TO CHECK
                pthread_mutex_lock(&data_mutex);
                User* u = user_list_head;
                while (u) {
                    if(strcmp(u->ip_addr, client_ip) == 0) {
                        strcpy(current_user, u->username);
                        break;
                    }
                    u = u->next;
                }
                pthread_mutex_unlock(&data_mutex);
            }
            
            if (strcmp(command, "LIST_USERS") == 0) {
                handle_list_users(sock);
            }
            else if (strcmp(command, "VIEW") == 0) {
                char* flags = strtok(NULL, ";\n");
                handle_view(sock, flags, current_user);
            }
            else if (strcmp(command, "INFO") == 0) {
                char* filename = strtok(NULL, ";\n");
                if (filename) handle_info(sock, filename);
            }
             else if (strcmp(command, "ADDACCESS") == 0) {
                char* filename = strtok(NULL, ";\n");
                char* user = strtok(NULL, ";\n");
                char* perm = strtok(NULL, ";\n");
                if (filename && user && perm) handle_add_access(sock, filename, user, perm);
            }
            else if (strcmp(command, "CREATE") == 0) {
                char* filename = strtok(NULL, ";\n");
                if (filename) handle_create(sock, filename, current_user);
            }
            else if (strcmp(command, "READ") == 0) {
                char* filename = strtok(NULL, ";\n");
                if (filename) handle_read(sock, filename, current_user);
            }
            else if (strcmp(command, "WRITE") == 0) {
                char* filename = strtok(NULL, ";\n");
                char* sent_num_str = strtok(NULL, ";\n");
                if (filename && sent_num_str) {
                    handle_write(sock, filename, atoi(sent_num_str), current_user);
                }
            }
            else if (strcmp(command, "DELETE") == 0) {
                char* filename = strtok(NULL, ";\n");
                if (filename) handle_delete(sock, filename, current_user);
            }
            else if (strcmp(command, "DELETE_METADATA") == 0) {
                char* filename = strtok(NULL, ";\n");
                if (filename) handle_delete_metadata(sock, filename, current_user);
            }
            else if (strcmp(command, "REGISTER_SS") != 0 && strcmp(command, "REGISTER_CLIENT") != 0) {
                char response[] = "ERROR: Unknown command.\n__END__\n";
                send(sock, response, strlen(response), 0);
            }
        }
    }
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


void register_storage_server(const char* ip, int port) {
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
    
    pthread_mutex_unlock(&data_mutex);
}


void handle_list_users(int sock) {
    char response[MAX_BUFFER_SIZE * 2] = ""; //bigger response buffer to account for longer responses
    
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
    int show_details = (flags && (strstr(flags, "l") != NULL));

    pthread_mutex_lock(&data_mutex);

    if (show_details) {
         snprintf(response, sizeof(response), "| %-20s | %-12s | %-17s |\n", "Filename", "Owner", "Location (SS)");
         strcat(response, "---------------------------------------------------------\n");
    }

    for (FileMetadata* current = file_list_head; current != NULL; current = current->next) {
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
    pthread_mutex_unlock(&data_mutex);
    
    strcat(response, "__END__\n");
    send(sock, response, strlen(response), 0);
}


void handle_info(int sock, const char* filename) {
    char response[1024];
    snprintf(response, sizeof(response), "INFO for '%s' is not yet implemented.\n__END__\n", filename);
    send(sock, response, strlen(response), 0);
}

void handle_add_access(int sock, const char* filename, const char* user, const char* perm) {
    // TODO: Find the file, add user to its access_list.
    char response[1024];
    snprintf(response, sizeof(response), "ADDACCESS for '%s' is not yet implemented.\n__END__\n", filename);
    send(sock, response, strlen(response), 0);
}

void handle_rem_access(int sock, const char* filename, const char* user) {
     // TODO: Find the file, remove user from its access_list.
    char response[1024];
    snprintf(response, sizeof(response), "REMACCESS for '%s' is not yet implemented.\n__END__\n", filename);
    send(sock, response, strlen(response), 0);
}


int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    pthread_mutex_init(&data_mutex, NULL);

    // 1. Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Could not create socket");
        return 1;
    }

    // 2. Prepare the sockaddr_in structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(NAME_SERVER_PORT);

    // 3. Bind
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    printf("[Name Server] Bind successful on port %d.\n", NAME_SERVER_PORT);

    // 4. Listen
    listen(server_sock, 10);
    printf("[Name Server] Waiting for incoming connections...\n");

    // 5. Accept and handle connections
    while ((client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len))) {
        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        pthread_t thread_id;
        client_info_t* client_info = malloc(sizeof(client_info_t));
        client_info->client_socket = client_sock;
        client_info->client_addr = client_addr;

        if (pthread_create(&thread_id, NULL, handle_connection, (void*)client_info) < 0) {
            perror("could not create thread");
            return 1;
        }
        
        //to prevent leakage of resources
        pthread_detach(thread_id);
    }

    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }
    
    pthread_mutex_destroy(&data_mutex);

    return 0;
}