# Docs++: Distributed File System

A sophisticated distributed file system implementation enabling secure, concurrent document collaboration with centralized coordination, persistent storage, and advanced file management capabilities.

---

## 📋 Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Core Components](#core-components)
4. [Features](#features)
5. [System Requirements](#system-requirements)
6. [Installation & Setup](#installation--setup)
7. [Building the Project](#building-the-project)
8. [Running the System](#running-the-system)
9. [User Commands Reference](#user-commands-reference)
10. [Communication Protocol](#communication-protocol)
11. [Error Codes](#error-codes)
12. [Design Decisions](#design-decisions)
13. [Project Structure](#project-structure)
14. [Testing](#testing)
15. [Development Phases](#development-phases)
16. [Troubleshooting](#troubleshooting)

---

## Overview

**Docs++** is a distributed file system designed for collaborative document editing and management. It implements a client-server architecture with a central Name Server coordinating between multiple Storage Servers and User Clients. The system supports concurrent read/write operations with sentence-level locking, persistent file storage, access control, and comprehensive logging.

### Key Highlights

- **Concurrent Access**: Multiple users can read/write different parts of files simultaneously
- **Sentence-Level Locking**: Prevents race conditions while allowing maximum parallelism
- **Persistent Storage**: All files and metadata survive server restarts
- **Access Control**: Fine-grained permission management for file access
- **Comprehensive Logging**: Detailed tracking of all operations for debugging and monitoring
- **Efficient Metadata Management**: O(1) file lookups using hash tables

---

## System Architecture

The system follows a **three-tier distributed architecture**:

```
┌─────────────────────────────────────────────────────────────┐
│                        User Clients (UC)                     │
│  (Interactive CLI for file operations & management)          │
└──────────────────────┬──────────────────────────────────────┘
                       │ TCP Sockets
                       │ (Port-based)
                       │
┌──────────────────────┴──────────────────────────────────────┐
│                    Name Server (NM)                          │
│  • Central Coordinator                                       │
│  • File Metadata Management (Hash Table)                     │
│  • Access Control List (ACL) Management                      │
│  • Multi-threaded Request Handler                            │
│  • Storage Server Routing                                    │
└──────────────────────┬──────────────────────────────────────┘
                       │ TCP Sockets
                       │ (Registration & Commands)
                       │
┌──────────────────────┴──────────────────────────────────────┐
│          Storage Servers (SS) [Multiple Instances]           │
│  • File Persistence (Disk Storage)                           │
│  • Sentence-Level Locking                                    │
│  • Direct Client Connections (Read/Write/Stream)            │
│  • Undo Management                                           │
│  • Data Integrity Guarantees                                 │
└─────────────────────────────────────────────────────────────┘
```

---

## Core Components

### 1. Name Server (NM)

**Location**: `src/name_server/name_server.c`

The central coordinator of the system responsible for:
- Accepting registrations from Storage Servers and User Clients
- Maintaining metadata about files and their locations
- Routing client requests to appropriate Storage Servers
- Handling file creation, deletion, and access control operations
- Managing user sessions and permissions

**Key Features**:
- Multi-threaded architecture using POSIX threads
- In-memory hash table for O(1) file lookups
- Mutex-protected shared data structures for thread safety
- Request-response communication protocol

**Data Structures**:
- Hash table mapping filenames to storage locations
- Linked list of registered users
- ACL (Access Control List) for each file
- File metadata cache

### 2. Storage Server (SS)

**Location**: `src/storage_server/storage_server.c`

Responsible for:
- Physically storing files on disk
- Executing create/delete operations
- Handling direct client connections for read/write/stream operations
- Managing sentence-level locks for concurrent writes
- Maintaining undo history
- Persisting file data across restarts

**Key Features**:
- TCP server for client connections
- File I/O operations (create, read, write, delete)
- Sentence segmentation and parsing
- Temporary file management for atomic writes
- Undo buffer management

**Data Structures**:
- File system directory structure
- Sentence lock table (per file)
- Undo buffer (previous file state)
- Word/sentence index mappings

### 3. User Client (UC)

**Location**: `src/client/user_client.c`

Provides:
- Interactive command-line interface
- User authentication and registration
- Command parsing and validation
- Direct connections to Storage Servers for performance-critical operations
- Human-readable output formatting

**Key Features**:
- Interactive command loop
- Multi-line command support for WRITE operations
- Response parsing and display
- Connection management

---

## Features

### User Functionalities (150 Points)

#### 1. **VIEW Files** [10 Points]
```
VIEW              # Lists all files user has access to
VIEW -a           # Lists all files on the system
VIEW -l           # Lists accessible files with details (table format)
VIEW -al          # Lists all system files with details
```
Displays files with metadata including word count, character count, last access time, and owner.

#### 2. **READ File** [10 Points]
```
READ <filename>   # Displays the complete file content
```
Retrieves and displays full file content to the user.

#### 3. **CREATE File** [10 Points]
```
CREATE <filename> # Creates an empty file
```
Creates a new empty file in the system. Returns error if file already exists.

#### 4. **WRITE to File** [30 Points]
```
WRITE <filename> <sentence_number>
<word_index> <content>
<word_index> <content>
...
ETIRW           # Complete the write operation
```
Advanced sentence-level editing with:
- Atomic multi-word updates within a sentence
- Automatic sentence re-segmentation on delimiter insertion
- Concurrent write protection via sentence locks
- Temporary file buffering for data integrity

#### 5. **UNDO Change** [15 Points]
```
UNDO <filename>   # Reverts the last change to the file
```
Single-level undo functionality. Reverts the most recent modification regardless of which user made it.

#### 6. **INFO About File** [10 Points]
```
INFO <filename>   # Displays comprehensive file metadata
```
Shows:
- Owner
- Creation timestamp
- Last modification timestamp
- Last access timestamp
- File size (bytes)
- Word and character counts
- Access permissions

#### 7. **DELETE File** [10 Points]
```
DELETE <filename> # Deletes the file from system
```
Removes file permanently. Only owner can delete. Updates all metadata and ACLs.

#### 8. **STREAM Content** [15 Points]
```
STREAM <filename> # Streams file content word-by-word with 0.1s delay
```
Direct connection to Storage Server for streaming with:
- Word-by-word display
- 0.1-second delays between words
- Error handling for server failures mid-stream

#### 9. **LIST Users** [10 Points]
```
LIST              # Lists all registered users in the system
```
Displays all currently registered users.

#### 10. **Access Control** [15 Points]
```
ADDACCESS -R <filename> <username>  # Add read access
ADDACCESS -W <filename> <username>  # Add write access (implies read)
REMACCESS <filename> <username>     # Remove all access
```
File owner can manage user permissions with:
- Separate read and write permissions
- Owner always has RW access
- Inherited permissions for write access

#### 11. **EXEC File** [15 Points]
```
EXEC <filename>   # Execute file as shell commands
```
Execute file content as shell script with output captured and displayed on Name Server.

### System Requirements (40 Points)

#### 1. **Data Persistence** [10 Points]
- All files stored on Storage Server disk
- Metadata persisted across restarts
- ACL information preserved
- Automatic reload on Storage Server startup

#### 2. **Access Control** [5 Points]
- Owner-based file ownership
- Read/Write permission management
- Permission enforcement on all operations
- Clear error messages for unauthorized access

#### 3. **Comprehensive Logging** [5 Points]
- Timestamp-based logging on NM and SS
- Request/response logging
- User and IP tracking
- Operation outcome logging
- Log format: `[TIMESTAMP] [SOURCE] [LEVEL] MESSAGE`

#### 4. **Error Handling** [5 Points]
- Universal error codes across system
- Informative error messages
- Graceful failure handling
- Client feedback on all failures

#### 5. **Efficient Search** [15 Points]
- O(1) average-case file lookup via hash table
- Cached metadata for recent operations
- LRU cache for frequently accessed files
- Quick permission verification

---

## System Requirements

### Software Requirements
- **OS**: Linux/Unix-based system
- **Compiler**: GCC (C99 or later)
- **Libraries**:
  - POSIX Threads (pthread)
  - Standard C Library (libc)
  - System socket libraries

### Hardware Requirements
- Minimum 512 MB RAM
- 100 MB disk space for file storage
- Network interface (localhost or LAN)

### Network
- TCP/IP networking support
- Default ports (configurable):
  - Name Server: 8080
  - Storage Servers: 8888+
  - Client connections: Dynamic

---

## Installation & Setup

### Prerequisites
```bash
# On Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential gcc make

# On macOS
# Install Xcode Command Line Tools
xcode-select --install
```

### Clone Repository
```bash
git clone https://github.com/CS3-OSN-Monsoon-2025/course-project-lostinpackets.git
cd course-project-lostinpackets
```

### Directory Structure Overview
```
course-project-lostinpackets/
├── src/
│   ├── error_codes.h              # Universal error code definitions
│   ├── logger.h                   # Logging utilities
│   ├── client/
│   │   ├── user_client.c          # User client implementation
│   │   ├── client_SS_helper_functions.c  # Storage server interaction helpers
│   │   └── user_client            # Compiled executable
│   ├── name_server/
│   │   ├── name_server.c          # Name server implementation
│   │   ├── CRWD.c                 # Create/Read/Write/Delete logic
│   │   ├── hash_table.h           # Hash table for metadata
│   │   ├── types.h                # Type definitions
│   │   └── name_server            # Compiled executable
│   └── storage_server/
│       ├── storage_server.c       # Storage server implementation
│       └── storage_server         # Compiled executable
├── ss_files/                      # Storage server data directory
├── bin/                           # Compiled binaries
├── testing/
│   ├── test_phase1.py            # Phase 1 test suite
│   └── test_phase2.py            # Phase 2 test suite
├── Makefile                       # Build configuration
└── README.md                      # This file
```

---

## Building the Project

### Using Makefile (Recommended)

```bash
# From project root directory
make clean    # Clean previous builds (optional)
make          # Build all components
```

This generates executables in the `bin/` directory:
- `bin/name_server`
- `bin/storage_server`
- `bin/user_client`

### Manual Compilation

```bash
# Build Name Server
gcc -g -Wall -pthread src/name_server/name_server.c -o bin/name_server

# Build Storage Server
gcc -g -Wall -pthread src/storage_server/storage_server.c -o bin/storage_server

# Build User Client
gcc -g -Wall src/client/user_client.c -o bin/user_client
```

### Build Flags Explanation
- `-g`: Include debug symbols
- `-Wall`: Enable all warnings
- `-pthread`: Link pthread library (required for multi-threading)

---

## Running the System

### Step-by-Step Startup

#### 1. Start the Name Server
```bash
# Terminal 1
./bin/name_server
# Output: Name Server listening on port 8080...
```

#### 2. Start Storage Server(s)
```bash
# Terminal 2
./bin/storage_server
# Output: Storage Server initialized, registering with NM...
```

You can start multiple Storage Servers in different terminals for distributed storage.

#### 3. Start User Client(s)
```bash
# Terminal 3+
./bin/user_client
# Prompt: Enter username:
```

Enter any username and start issuing commands.

### Quick Start Script
```bash
#!/bin/bash
# Start all components in background
./bin/name_server &
NM_PID=$!
sleep 1

./bin/storage_server &
SS_PID=$!
sleep 1

./bin/user_client
```

### Configuration

Edit the source files for custom settings:

**Name Server** (`src/name_server/name_server.c`):
```c
#define NM_PORT 8080  // Change Name Server port
```

**Storage Server** (`src/storage_server/storage_server.c`):
```c
#define SS_PORT 8888  // Change Storage Server port
```

**Client** (`src/client/user_client.c`):
```c
#define NM_IP "127.0.0.1"    // Change NM IP
#define NM_PORT 8080         // Change NM port
```

---

## User Commands Reference

### File Operations

#### Viewing Files
```
VIEW                    # Your accessible files
VIEW -a                 # All system files
VIEW -l                 # Your files with details
VIEW -al                # All files with details
```

**Example Output**:
```
-----------------------------------------
|  Filename  | Words | Chars | Owner |
|------------|-------|-------|-------|
| doc1.txt   |   150 |   892 | alice |
| doc2.txt   |   230 |  1450 | bob   |
-----------------------------------------
```

#### Reading
```
READ <filename>
```

**Example**:
```
READ document.txt
```

#### Creating
```
CREATE <filename>
```

**Example**:
```
CREATE notes.txt
```

#### Writing (Multi-step)
```
WRITE <filename> <sentence_number>
<word_index> <new_content>
<word_index> <new_content>
...
ETIRW
```

**Example**:
```
WRITE report.txt 0
0 This
1 is
2 a
3 test.
ETIRW
```

#### Undo
```
UNDO <filename>
```

Reverts the last modification to the file.

#### Streaming
```
STREAM <filename>
```

Displays file content word-by-word with 0.1-second delays.

#### Info
```
INFO <filename>
```

**Example Output**:
```
File: document.txt
Owner: alice
Created: 2025-11-20 10:30:00
Last Modified: 2025-11-25 14:22:30
Last Accessed: 2025-11-25 14:23:15
Size: 1,245 bytes
Words: 234
Characters: 1,245
Access: Read (alice, bob), Write (alice)
```

#### Deletion
```
DELETE <filename>
```

**Note**: Only file owner can delete.

#### User List
```
LIST
```

Shows all registered users.

#### Access Control
```
ADDACCESS -R <filename> <username>  # Add read
ADDACCESS -W <filename> <username>  # Add write
REMACCESS <filename> <username>     # Remove all
```

#### Execute
```
EXEC <filename>
```

Executes file as shell script (if you have read access).

---

## Communication Protocol

### Protocol Overview

The system uses a **semicolon-delimited text protocol** for reliability and debuggability:

```
COMMAND;ARG1;ARG2;ARG3;...
```

### Message Format

#### Client → Name Server
```
VIEW
LIST
CREATE;filename.txt
DELETE;filename.txt
INFO;filename.txt
ADDACCESS;-R;filename.txt;username
REMACCESS;filename.txt;username
```

#### Name Server → Client
**Single Response**:
```
OK;Data
or
ERROR;404;File not found
```

**Multi-line Response**:
```
RESPONSE_START
Line 1
Line 2
...
__END__
```

#### Client → Storage Server (Direct Connection)
```
READ;filename.txt
WRITE;filename.txt;sentence_number
word_updates...
ETIRW
STREAM;filename.txt
```

#### Storage Server → Name Server
```
REGISTER;IP;NM_PORT;CLIENT_PORT;files_list
ACK;OPERATION;filename.txt;status
```

### Example Session

**Scenario**: User "alice" creates and writes to a file

```
Client → NM: CREATE;hello.txt
NM → Client: OK;File created successfully
NM → SS: CREATE;hello.txt

SS → NM: ACK;CREATE;hello.txt;SUCCESS

Client → NM: WRITE;hello.txt;0
NM → Client: DIRECT;SS_IP;SS_PORT
Client → SS: WRITE;hello.txt;0
Client → SS: 0 Hello
Client → SS: 1 World
Client → SS: ETIRW
SS → Client: OK;Write successful
```

---

## Error Codes

### Universal Error Code Format
```
ERROR;CODE;Message
```

### Error Code Reference

| Code | Name | Meaning |
|------|------|---------|
| 400 | UNKNOWN_COMMAND | Command not recognized |
| 401 | NOT_OWNER | Only owner can perform this operation |
| 403 | PERMISSION_DENIED | User lacks required permissions |
| 404 | FILE_NOT_FOUND | Requested file doesn't exist |
| 405 | INVALID_SENTENCE_INDEX | Sentence index out of range |
| 406 | INVALID_WORD_INDEX | Word index out of range |
| 409 | FILE_EXISTS | File already exists |
| 422 | INVALID_ARGS | Arguments invalid or malformed |
| 503 | NO_SS_AVAILABLE | No Storage Server available |
| 504 | SS_FAILURE | Storage Server failure |
| 505 | SS_UNREACHABLE | Cannot connect to Storage Server |
| 105 | USER_NOT_FOUND | User not registered |
| 106 | INVALID_INPUT | Input format invalid |
| 107 | SERVER_MISC | Miscellaneous server error |
| 108 | SS_UNREACHABLE | Storage Server connection failed |

### Common Errors

```
ERROR;403;Permission Denied: You don't have write access to this file
ERROR;404;File Not Found: document.txt does not exist
ERROR;401;Not Owner: Only alice can delete this file
ERROR;405;Invalid Sentence Index: Sentence 5 does not exist (file has 3 sentences)
ERROR;422;Invalid Arguments: WRITE requires filename and sentence number
```

---

## Design Decisions

### 1. **Communication Protocol**
**Choice**: Semicolon-delimited text protocol
- **Rationale**: Human-readable for debugging, easy to parse in C, language-agnostic
- **Alternative Considered**: Binary protocol (more efficient but harder to debug)

### 2. **Concurrency Model**
**Choice**: POSIX threads (pthread) on Name Server
- **Rationale**: Lightweight, standard, allows handling multiple concurrent connections
- **Per-Connection**: Each client/server connection handled in separate thread
- **Synchronization**: Single global mutex protecting shared data

### 3. **Data Structures for Metadata**
**Choice**: Hash table for files, linked lists for users
- **Rationale**: O(1) average lookup for files, simple implementation
- **Hash Function**: Simple string hashing
- **Collision Handling**: Chaining with linked lists

### 4. **File Locking Strategy**
**Choice**: Sentence-level locking with temporary files
- **Rationale**: Maximum parallelism while preventing race conditions
- **Implementation**: 
  - Mutex per sentence during writes
  - Temporary swap file created during write
  - Atomic move to original file on ETIRW
- **Undo Support**: Previous state saved before each write

### 5. **Persistent Storage**
**Choice**: Direct filesystem storage in `ss_files/` directory
- **Rationale**: Simple, leverages OS file system, automatic persistence
- **Metadata Storage**: Separate metadata files (`.meta`) alongside data files
- **Recovery**: List directory on startup to recover files

### 6. **Error Handling**
**Choice**: Centralized error codes with descriptive messages
- **Rationale**: Consistent error reporting across system
- **Implementation**: Universal error code definitions in `error_codes.h`

### 7. **Logging**
**Choice**: Timestamp + Source + Level + Message format
- **Rationale**: Easy parsing, sufficient information for debugging
- **Output**: Direct to terminal (stdout)
- **Alternative**: Could be extended to file-based logging

---

## Project Structure

### Source Code Organization

```
src/
├── error_codes.h              # Error definitions
├── logger.h                   # Logging macros
├── client/
│   ├── user_client.c          # Main client loop, command parsing
│   ├── client_SS_helper_functions.c  # Direct SS communication
│   └── user_client            # Compiled binary
├── name_server/
│   ├── name_server.c          # Main NM event loop
│   ├── CRWD.c                 # Create/Read/Write/Delete handlers
│   ├── hash_table.h           # Hash table implementation
│   ├── types.h                # Type definitions (File, User, etc.)
│   └── name_server            # Compiled binary
└── storage_server/
    ├── storage_server.c       # Main SS event loop
    └── storage_server         # Compiled binary

ss_files/                       # Storage directory (created at runtime)
├── file1.txt
├── file1.txt.meta
├── file2.txt
├── file2.txt.meta
├── file2.txt.undo             # Undo backup
└── ...

bin/                            # Compiled executables
├── name_server
├── storage_server
└── user_client

testing/
├── test_phase1.py            # Registration tests
└── test_phase2.py            # VIEW/LIST tests
```

### Key Header Files

**error_codes.h**: Universal error code definitions
```c
#define ERR_FILE_NOT_FOUND 404
#define ERR_PERMISSION_DENIED 403
// ... more codes
```

**logger.h**: Logging utility functions
```c
log_message(LOG_INFO, "NM", "File created successfully");
log_message(LOG_ERROR, "SS", "Failed to write file");
```

**types.h**: Core data structures
```c
typedef struct File {
    char name[256];
    char owner[128];
    int word_count;
    // ...
} File;
```

**hash_table.h**: Hash table operations
```c
File* hash_table_lookup(const char* filename);
void hash_table_insert(const char* key, File* file);
```

---

## Testing

### Running Tests

#### Automated Test Suite
```bash
# Phase 1 tests (registration)
python3 testing/test_phase1.py

# Phase 2 tests (metadata operations)
python3 testing/test_phase2.py
```

#### Manual Testing

1. **Basic File Operations**
```bash
# Terminal 1: Start NM
./bin/name_server

# Terminal 2: Start SS
./bin/storage_server

# Terminal 3: Start Client
./bin/user_client
username: alice

# Test commands
alice> CREATE test.txt
alice> READ test.txt
alice> WRITE test.txt 0
alice> 0 Hello World!
alice> ETIRW
alice> READ test.txt
alice> DELETE test.txt
```

2. **Concurrent Operations**
```bash
# Multiple clients accessing simultaneously
./bin/user_client # User 1
./bin/user_client # User 2 (in another terminal)
```

3. **Access Control**
```bash
# User alice creates file
alice> CREATE secret.txt
alice> WRITE secret.txt 0
alice> 0 Secret content
alice> ETIRW
alice> ADDACCESS -R secret.txt bob

# User bob (in another terminal)
bob> READ secret.txt  # Should work (read)
bob> WRITE secret.txt 0  # Should fail (no write)
```

### Test Scenarios

| Scenario | Expected Result |
|----------|-----------------|
| Create, write, read file | File contents match written data |
| Multiple users write different sentences | Both writes succeed |
| Multiple users write same sentence | Second user blocked until first releases |
| UNDO after write | File reverted to previous state |
| INFO on file | All metadata displayed |
| Unauthorized write attempt | Permission denied error |
| Stream file | Content displayed word-by-word with delay |
| Server restart | Files persist, SS re-registers |

---

## Development Phases

### Phase 1: Foundation & Network (Completed)
- ✅ Project structure setup
- ✅ TCP socket communication
- ✅ Registration protocols
- ✅ Multi-threaded NM

### Phase 2: Core Logic & Metadata (Completed)
- ✅ In-memory hash table
- ✅ User/file registration
- ✅ VIEW and LIST commands
- ✅ Detailed file information

### Phase 3: Storage & Basic Operations (In Progress)
- ⚙️ File creation/deletion
- ⚙️ Data persistence
- ⚙️ READ operations
- ⚙️ ACL management

### Phase 4: Concurrent Writing (In Progress)
- ⚙️ Sentence-level locking
- ⚙️ WRITE command implementation
- ⚙️ Atomic writes via temp files
- ⚙️ Concurrent write tests

### Phase 5: Advanced Features (Planned)
- ⏳ UNDO functionality
- ⏳ STREAM operations
- ⏳ EXEC command
- ⏳ Access control enforcement

### Phase 6: Robustness (Planned)
- ⏳ Comprehensive logging
- ⏳ Error handling
- ⏳ Efficient search/caching
- ⏳ Edge case handling

### Phase 7: Integration & Testing (Planned)
- ⏳ End-to-end testing
- ⏳ Performance optimization
- ⏳ Documentation finalization
- ⏳ Final delivery

---

## Troubleshooting

### Common Issues & Solutions

#### 1. "Address already in use" Error
```
Error: bind failed: Address already in use
```

**Solution**: 
- Port is still in use by previous process
- Wait 30 seconds for socket TIME_WAIT or kill process:
  ```bash
  # Find process using port 8080
  lsof -i :8080
  # Kill the process
  kill -9 <PID>
  ```

#### 2. "Connection refused"
```
Error: Cannot connect to Name Server
```

**Solution**:
- Ensure NM is running first
- Check NM is listening: `netstat -tuln | grep 8080`
- Verify IP/port configuration

#### 3. "File not found" when writing
```
WRITE: File does not exist
```

**Solution**:
- Create file first: `CREATE filename.txt`
- Check spelling of filename

#### 4. "Permission denied" errors
```
ERROR;403;Permission Denied
```

**Solution**:
- Only file owner can delete
- Request read/write access first
- Check file ownership: `INFO filename.txt`

#### 5. Storage Server won't start
```
Storage Server failed to initialize
```

**Solution**:
- Check `ss_files/` directory exists and is writable
- Verify NM is running and accessible
- Check firewall isn't blocking connections

#### 6. Compilation errors
```
undefined reference to `pthread_create'
```

**Solution**:
- Ensure `-pthread` flag in compilation
- Using Makefile? Run `make clean && make`

### Debug Tips

#### Enable Verbose Logging
```bash
# Recompile with debug output (edit source files)
# Look for log_message() calls
gcc -g -Wall -pthread src/name_server/name_server.c -o bin/name_server
```

#### Monitor Network Activity
```bash
# Watch connections to Name Server
watch -n 1 'netstat -tuln | grep 8080'

# Monitor file access
watch -n 1 'ls -la ss_files/'
```

#### Test Individual Components
```bash
# Test Name Server only (no clients)
./bin/name_server
# Ctrl+C to stop

# Test Storage Server registration
./bin/storage_server
# Should show "Registered with NM"
```

---

## Performance Characteristics

### Expected Performance Metrics

| Operation | Complexity | Typical Time |
|-----------|-----------|--------------|
| File lookup | O(1) avg | < 1ms |
| User lookup | O(n) | < 10ms (n = users) |
| CREATE | O(1) | < 50ms |
| READ (small file) | O(1) | < 10ms |
| WRITE single word | O(m) | < 100ms (m = sentence length) |
| STREAM (all words) | O(w) | variable (0.1s per word) |
| UNDO | O(1) | < 50ms |

### Scalability Considerations

- **File limit**: Limited by disk space and filesystem
- **User limit**: Limited by NM memory (linked list O(n) lookup)
- **Concurrent connections**: Limited by OS file descriptor limit
- **Improvement opportunity**: Replace linked lists with hash table for users

---

## Contributing & Future Enhancements

### Bonus Features (Not Yet Implemented)

1. **Hierarchical Folder Structure** (15 points)
   - Create/navigate folder hierarchies
   - Move files between folders

2. **Checkpoints** (15 points)
   - Save file snapshots with tags
   - Revert to specific checkpoints

3. **Access Requests** (5 points)
   - Users can request file access
   - Owner approves/denies requests

4. **Fault Tolerance** (15 points)
   - Data replication across Storage Servers
   - Failure detection and recovery
   - Sync on reconnection

5. **Unique Innovation Factor** (5 points)
   - Implement and document unique feature

---

## Support & Contact

For issues or questions:
1. Check this README's Troubleshooting section
2. Review test cases in `testing/` directory
3. Check error codes and logs
4. Contact course instructors

---

## Acknowledgments

- POSIX specification for threading and sockets
- Course instructors and teaching assistants
- Project specifications and rubric guidance

---

**Last Updated**: November 25, 2025  
**Status**: Phase 2 Complete, Phases 3-7 In Progress  
**Version**: 1.0 (MVP)