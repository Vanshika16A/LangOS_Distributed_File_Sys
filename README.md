# Docs++: Phase 1 Completion Report

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

