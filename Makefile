# Compiler and flags
CC = clang++
CFLAGS = -std=c++23 -Wall -Wextra -g -O3
LDFLAGS =

# Directories
SRC_DIR = src
BUILD_DIR = build

# Files
SRC = $(SRC_DIR)/*.cpp
BIN = $(BUILD_DIR)/opencmd.exe
SHELL = cmd.exe

# Default target
all: $(BIN)

# Build rule
$(BIN): $(SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	@if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"

# Clean build files
clean:
	@if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"

# Convenience target to run the program
run: all
	$(BIN)

.PHONY: all clean run
