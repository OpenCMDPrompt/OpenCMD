# Compiler and flags
CC = g++
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lreadline

# Directories
SRC_DIR = src
BUILD_DIR = build

# Files
SRC = $(SRC_DIR)/*.cpp
BIN = $(BUILD_DIR)/opencmd.exe

# Default target
all: $(BIN)

# Build rule
$(BIN): $(SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean build files
clean:
	rm -rf $(BUILD_DIR)

# Convenience target to run the program
run: all
	./$(BIN)

.PHONY: all clean run
