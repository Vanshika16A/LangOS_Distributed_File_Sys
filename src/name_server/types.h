#ifndef TYPES_H
#define TYPES_H

#include <time.h>
#include <netinet/in.h> // Required for the sockaddr_in struct definition

// --- FORWARD DECLARATION ---
// This is a crucial technique. It tells the compiler:
// "Trust me, a struct named 'StorageServer' will be fully defined later."
// This allows FileMetadata to contain a pointer to a StorageServer
// without needing the full StorageServer definition yet, breaking the circular dependency.
struct StorageServer;

// --- STRUCT DEFINITIONS ---

// Describes a user with permission on a specific file
typedef struct AccessNode {
    char username[50];
    char permission; // 'R' for read, 'W' for write
    struct AccessNode* next;
} AccessNode;
typedef struct RequestNode {
    char username[50];
    struct RequestNode* next;
} RequestNode;

// Contains all metadata for a single file
typedef struct FileMetadata {
    char filename[100];
    int is_directory; // 1 if directory, 0 if regular file
    char owner[50];
    int word_count;
    int char_count;
    time_t last_access;
    struct StorageServer* ss; // Pointer to the SS that holds this file
    AccessNode* access_list;
    struct FileMetadata* next; // Pointer for the main linked list
    RequestNode* pending_requests;
    // --- UNIQUE FEATURE ---
    char annotation[256]; // Stores the sticky note
    // ----------------------
} FileMetadata;

// Describes a registered Storage Server
typedef struct StorageServer {
    char ip_addr[20];
    int port;
    struct StorageServer* next;
} StorageServer;

// Describes a registered user
typedef struct User {
    char username[50];
    char ip_addr[20];
    struct User* next;
} User;

// Bundles information about a connected client for passing to a thread
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} client_info_t;

#endif // TYPES_H