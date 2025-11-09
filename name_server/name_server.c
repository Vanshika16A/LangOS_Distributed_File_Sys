#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define MAX_BUFFER_SIZE 1024
#define NAME_SERVER_PORT 8080

// A struct to pass to our thread function
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} client_info_t;
// Represents a user with access to a file
typedef struct AccessNode {
    char username[50];
    char permission; // 'R' for read, 'W' for write
    struct AccessNode* next;
} AccessNode;

// Represents file metadata
typedef struct FileMetadata {
    char filename[100];
    char owner[50];
    int word_count;
    int char_count;
    time_t last_access;
    // We'll store which SS has the file later
    AccessNode* access_list;
    struct FileMetadata* next;
} FileMetadata;

// Represents a connected user
typedef struct User {
    char username[50];
    char ip_addr[20];
    struct User* next;
} User;
// --- 2. GLOBAL VARIABLES ---
FileMetadata* file_list_head = NULL;
User* user_list_head = NULL;
pthread_mutex_t data_mutex; // Mutex to protect our global lists
// --- FUNCTION PROTOTYPES ---
void register_user(const char* username, const char* ip_addr);
void register_storage_server(const char* files_str);
void handle_list_users(int sock);
void handle_view(int sock, const char* flags, const char* username);
void handle_info(int sock, const char* filename);
void handle_add_access(int sock, const char* filename, const char* user, const char* perm);
void handle_rem_access(int sock, const char* filename, const char* user);
// --- END OF PROTOTYPES ---
void* handle_connection(void* client_info_p) {
    client_info_t* client_info = (client_info_t*)client_info_p;
    int sock = client_info->client_socket;
    char* client_ip = inet_ntoa(client_info->client_addr.sin_addr);
    
    char buffer[MAX_BUFFER_SIZE];
    int read_size;
    char current_user[50] = "anonymous"; // Track user for this session
    // Read the message from the client/server
    if ((read_size = recv(sock, buffer, MAX_BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        char* command = strtok(buffer, ";\n");

        if (command != NULL) {
            printf("[Name Server] Received Command: %s\n", command);

            if (strcmp(command, "REGISTER_SS") == 0) {
                strtok(NULL, ";\n"); // Skip SS IP
                strtok(NULL, ";\n"); // Skip SS Port
                char* files = strtok(NULL, ";\n");
                if (files) register_storage_server(files);

                // ADD THIS BLOCK TO SEND A RESPONSE
                char response[] = "ACK_SS_REG\n__END__\n";
                send(sock, response, strlen(response), 0);
            } 
            else if (strcmp(command, "REGISTER_CLIENT") == 0) {
                char* username = strtok(NULL, ";\n");
                if (username) {
                    register_user(username, client_ip);
                    strcpy(current_user, username); // Set user for this session
                }

                // ADD THIS BLOCK TO SEND A RESPONSE
                char response[] = "ACK_CLIENT_REG\n__END__\n";
                send(sock, response, strlen(response), 0);
            }
            else if (strcmp(command, "LIST_USERS") == 0) {
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
            // ... add other command handlers here ...
            else {
                char response[] = "ERROR: Unknown command.\n__END__\n";
                send(sock, response, strlen(response), 0);
            }
        }
    }
    // ... (keep cleanup code from Phase 1) ...
}
// --- HELPER FUNCTIONS FOR REGISTRATION ---

void register_user(const char* username, const char* ip_addr) {
    pthread_mutex_lock(&data_mutex);

    // Create new user node
    User* newUser = (User*)malloc(sizeof(User));
    strcpy(newUser->username, username);
    strcpy(newUser->ip_addr, ip_addr);
    newUser->next = user_list_head;
    user_list_head = newUser;

    printf("[Data] Registered user '%s' from IP %s\n", username, ip_addr);
    pthread_mutex_unlock(&data_mutex);
}

// In a real system, SS registration would be more complex. For now,
// we just use it to populate the file list.
void register_storage_server(const char* files_str) {
    pthread_mutex_lock(&data_mutex);
    
    char files_copy[1024];
    strcpy(files_copy, files_str);

    char* file_token = strtok(files_copy, ",");
    while (file_token != NULL) {
        // Check if file already exists to avoid duplicates
        int found = 0;
        for (FileMetadata* current = file_list_head; current != NULL; current = current->next) {
            if (strcmp(current->filename, file_token) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            FileMetadata* newFile = (FileMetadata*)malloc(sizeof(FileMetadata));
            strcpy(newFile->filename, file_token);
            strcpy(newFile->owner, "system"); // Default owner
            newFile->word_count = 100; // Dummy data
            newFile->char_count = 500; // Dummy data
            newFile->last_access = time(NULL);
            newFile->access_list = NULL; // No users have access yet
            newFile->next = file_list_head;
            file_list_head = newFile;
            printf("[Data] Registered file '%s'\n", file_token);
        }
        file_token = strtok(NULL, ",");
    }
    pthread_mutex_unlock(&data_mutex);
}
// --- 5. IMPLEMENT COMMAND HANDLERS ---

void handle_list_users(int sock) {
    char response[MAX_BUFFER_SIZE * 2] = ""; // Larger buffer for response
    
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
    // In a real system, VIEW would filter based on username access.
    // For now, -a shows all, and default shows all.
    char response[MAX_BUFFER_SIZE * 4] = "";
    
    int show_details = (flags && (strstr(flags, "l") != NULL));

    pthread_mutex_lock(&data_mutex);

    if (show_details) {
         snprintf(response, sizeof(response), "| %-20s | %-8s | %-8s | %-10s |\n", "Filename", "Words", "Chars", "Owner");
         strcat(response, "-------------------------------------------------------------\n");
    }

    for (FileMetadata* current = file_list_head; current != NULL; current = current->next) {
        char line[256];
        if (show_details) {
            snprintf(line, sizeof(line), "| %-20s | %-8d | %-8d | %-10s |\n", 
                current->filename, current->word_count, current->char_count, current->owner);
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

// NOTE: Stubs for other handlers. Implementation would be similar.
void handle_info(int sock, const char* filename) {
    // TODO: Find the file, format its details, send back.
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
    listen(server_sock, 5);
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
        // We don't join the thread because we want to handle multiple connections at once.
        // This is called a "detached" thread model in principle.
    }

    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;
}