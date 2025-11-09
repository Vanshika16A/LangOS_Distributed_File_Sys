# Docs++: 
## Phase 1 Completion Report

This document outlines the progress and implementation details for Phase 1 of the Docs++ project.

### Implemented Features

1.  **Core Component Initialization**: Basic server shells for the Name Server (NM) and client stubs for the Storage Server (SS) and User Client (UC) have been created using TCP sockets.
2.  **Name Server Concurrency**: The Name Server is capable of handling multiple simultaneous incoming connections using the POSIX Threads (`pthreads`) library. Each incoming connection is handled in a separate thread.
3.  **Registration Protocol**: A simple, text-based communication protocol has been defined and implemented for the initial registration of components.
    *   **Storage Server Registration**: An SS can connect to the NM and register its IP address, a dedicated client-facing port, and a list of files it possesses.
    *   **User Client Registration**: A UC can connect to the NM and register its username. The NM automatically captures its IP address from the connection details.

### Important Design Choices

*   **Communication Protocol**: We have chosen a simple, semicolon-delimited text protocol (`COMMAND;ARG1;ARG2;...`). This choice prioritizes ease of implementation, debugging (messages are human-readable), and extensibility for future commands.
*   **Concurrency Model**: The Name Server uses a multi-threaded architecture (`pthread_create` for each connection). This is a crucial design choice to ensure the NM is non-blocking and can serve multiple SS and UC entities concurrently, which is a core requirement of the system.
*   **Stateless Connections**: For Phase 1, all connections are short-lived. A component connects, sends its registration message, and disconnects. This simplifies the initial logic.

### Libraries Used

The implementation for Phase 1 relies on standard POSIX C libraries:
*   `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<unistd.h>`: For standard I/O, memory, and system calls.
*   `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`: For TCP socket programming (BSD sockets API).
*   `<pthread.h>`: For multi-threading support in the Name Server.

### How to Compile and Run

1.  **Compile all components:**
    ```bash
    # From the root directory
    gcc name_server/name_server.c -o name_server/name_server -pthread
    gcc storage_server/storage_server.c -o storage_server/storage_server
    gcc client/user_client.c -o client/user_client
    ```

2.  **Run the system:**
    *   **Start Name Server (in one terminal):**
        ```bash
        ./name_server/name_server
        ```
    *   **Start Storage Server (in a second terminal):**
        ```bash
        ./storage_server/storage_server
        ```
    *   **Start User Client (in a third terminal):**
        ```bash
        ./client/user_client
        # Then enter a username when prompted
        ```

### Test Cases Executed

The following test scenarios were successfully executed to validate the Phase 1 implementation.

| Test Case ID | Description                                                    | Components Involved | Status   |
|--------------|----------------------------------------------------------------|---------------------|----------|
| P1-T01       | Single Storage Server registration.                            | NM, SS              | ✅ Pass  |
| P1-T02       | Single User Client registration.                               | NM, UC              | ✅ Pass  |
| P1-T03       | Multiple, concurrent registrations from both SS and UC.        | NM, SS, UC          | ✅ Pass  |
| P1-T04       | Automated test script simulating 2 SS and 2 UC registrations.  | NM, Python Script   | ✅ Pass  |

In all cases, the Name Server correctly received, parsed, and logged the registration details for all connecting components, demonstrating a robust foundation for the next phases.

---
## Phase 2 Completion Report

Phase 2 builds upon the basic network foundation by implementing the core logic and state management within the Name Server.

### Implemented Features

1.  **In-Memory Metadata Storage**: The Name Server now maintains state. It uses in-memory linked lists to store information about registered users and available files.
2.  **Concurrency Control**: Critical sections of code where shared data structures (the linked lists) are modified are now protected by a `pthread_mutex`. This prevents race conditions and ensures data integrity in the multi-threaded environment.
3.  **Interactive User Client**: The client is no longer a single-shot application. It now features an interactive command-line loop, allowing users to issue multiple commands in a single session.
4.  **Direct Command Handling**: The Name Server can now directly handle and respond to the following client commands:
    *   `LIST`: Lists all registered users.
    *   `VIEW`: Lists all available files. Supports a `-l` flag to show detailed information like word count, character count, and owner.

### Important Design Choices

*   **Data Structures**: We have used **singly linked lists** as our primary in-memory data structures. While not the most performant for lookups (O(n)), they are simple to implement in C without external libraries and are sufficient for the current scale of the project. This choice prioritizes implementation simplicity.
*   **Concurrency Control**: A **single, global mutex** (`pthread_mutex_t`) is used to protect all shared data. This coarse-grained locking strategy is simple to implement and is effective at preventing all race conditions, though it may limit true parallelism in future, more complex scenarios.
*   **Request-Response Protocol**: The communication model has been upgraded to a full request-response pattern. A special token, `__END__`, is used to signify the end of a multi-line server response, making the client's receiving logic robust.

### Test Cases Executed

The following test scenarios were successfully executed to validate Phase 2.

| Test Case ID | Description                                                    | Status   |
|--------------|----------------------------------------------------------------|----------|
| P2-T01       | NM stores and retains SS and UC registration data.             | ✅ Pass  |
| P2-T02       | Client `LIST` command correctly returns a list of all users.   | ✅ Pass  |
| P2-T03       | Client `VIEW` command correctly returns a list of all files.   | ✅ Pass  |
| P2-T04       | Client `VIEW -l` command returns a detailed, formatted table.  | ✅ Pass  |
| P2-T05       | Data is consistent when accessed by multiple concurrent clients. | ✅ Pass  |
