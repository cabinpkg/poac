# Compiler settings
CC = clang++
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O3 -DNDEBUG
CFLAGS = -Wall -Wextra -fdiagnostics-color -pedantic-errors -std=c++20 $(DEBUG_FLAGS)
LDFLAGS = -L.

# Archiver settings
AR = ar
ARFLAGS = rcs

# Directories
OUT_DIR = build-out

# Project settings
PROJ_NAME = $(OUT_DIR)/poac


all: $(OUT_DIR) $(OUT_DIR)/Cmd $(PROJ_NAME)

clean:
	rm -rf $(OUT_DIR)

$(OUT_DIR):
	mkdir -p $@

$(OUT_DIR)/Cmd:
	mkdir -p $@

$(PROJ_NAME): $(OUT_DIR)/Cmd/Build.o $(OUT_DIR)/Logger.o $(OUT_DIR)/TermColor.o $(OUT_DIR)/main.o
	$(CC) $(CFLAGS) $^ -o $@

$(OUT_DIR)/TermColor.o: src/TermColor.cc src/TermColor.hpp
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/Logger.o: src/Logger.cc src/Logger.hpp
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/Cmd/Build.o: src/Cmd/Build.cc src/Cmd/Build.hpp src/Rustify.hpp src/Algos.hpp
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/main.o: src/main.cc src/Cmd/Build.hpp
	$(CC) $(CFLAGS) -c $< -o $@


test: $(OUT_DIR)/tests $(OUT_DIR)/tests/cmd_build
	$(OUT_DIR)/tests/cmd_build

$(OUT_DIR)/tests:
	mkdir -p $@

$(OUT_DIR)/tests/cmd_build: $(OUT_DIR)/tests/cmd_build.o $(OUT_DIR)/Build.o $(OUT_DIR)/Logger.o $(OUT_DIR)/TermColor.o
	$(CC) $(CFLAGS) $^ -o $@

$(OUT_DIR)/tests/cmd_build.o: tests/cmd_build.cc
	$(CC) $(CFLAGS) -c $< -o $@


.PHONY: all clean test
