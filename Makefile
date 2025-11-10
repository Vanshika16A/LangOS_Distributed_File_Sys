# Makefile for the Distributed File System

# Compiler
CC = gcc

# Compiler flags
# -g: Add debug symbols
# -Wall: Turn on all warnings
CFLAGS = -g -Wall

# Linker flags
# -pthread: Required for multi-threaded applications
LDFLAGS = -pthread

# --- NEW STRUCTURE ---
# Directory for compiled executables
BIN_DIR = bin

# Directories for source code
SRC_DIR = src
CLIENT_DIR = $(SRC_DIR)/client
NS_DIR = $(SRC_DIR)/name_server
SS_DIR = $(SRC_DIR)/storage_server

# Source files
NS_SRC = $(NS_DIR)/name_server.c
SS_SRC = $(SS_DIR)/storage_server.c
CLIENT_SRC = $(CLIENT_DIR)/user_client.c

# Executable targets (now inside bin/)
NS_EXE = $(BIN_DIR)/name_server
SS_EXE = $(BIN_DIR)/storage_server
CLIENT_EXE = $(BIN_DIR)/user_client

# Default target: build all executables
all: $(NS_EXE) $(SS_EXE) $(CLIENT_EXE)

# --- UPDATED BUILD RULES ---

# Rule to build the Name Server
# It depends on its source file and will create the bin/ dir if needed
$(NS_EXE): $(NS_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# Rule to build the Storage Server
$(SS_EXE): $(SS_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# Rule to build the User Client
$(CLIENT_EXE): $(CLIENT_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

# This is an order-only prerequisite, it creates the bin directory
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Target to clean up build files
clean:
	# Remove the entire bin directory and its contents
	rm -rf $(BIN_DIR)

.PHONY: all clean