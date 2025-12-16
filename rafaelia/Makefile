# Rafaelia Module Makefile
# Copyright (c) 2024 Rafael Melo Reis
# Licensed under RAFAELIA MODULE LICENSE

.POSIX:
.SUFFIXES:

# Compiler and flags
CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -pedantic -fvisibility=hidden
LDFLAGS = 
PREFIX = /usr/local
DESTDIR =

# Detect platform
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Platform-specific adjustments
# Only add -lrt for native Linux builds (not Android)
ifndef ANDROID_NDK
ifeq ($(UNAME_S),Linux)
	LDFLAGS += -lrt
endif
endif

ifeq ($(UNAME_S),Darwin)
	# macOS specific flags
	CFLAGS += -D_DARWIN_C_SOURCE
endif

# Android NDK support
ifdef ANDROID_NDK
	NDK_HOST_TAG = linux-x86_64
	ifeq ($(UNAME_S),Darwin)
		NDK_HOST_TAG = darwin-x86_64
	endif
	
	NDK_API_LEVEL ?= 21
	NDK_ARCH ?= arm64
	
	ifeq ($(NDK_ARCH),arm64)
		NDK_TARGET = aarch64-linux-android$(NDK_API_LEVEL)
	else ifeq ($(NDK_ARCH),arm)
		NDK_TARGET = armv7a-linux-androideabi$(NDK_API_LEVEL)
	else ifeq ($(NDK_ARCH),x86_64)
		NDK_TARGET = x86_64-linux-android$(NDK_API_LEVEL)
	else ifeq ($(NDK_ARCH),x86)
		NDK_TARGET = i686-linux-android$(NDK_API_LEVEL)
	endif
	
	CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(NDK_HOST_TAG)/bin/clang
	CFLAGS += --target=$(NDK_TARGET)
endif

# Build targets
BIN_DIR = bin
OBJ_DIR = obj
TARGETS = $(BIN_DIR)/rafaelia_arch_probe

# Source files
SOURCES = rafaelia_arch_probe.c
OBJECTS = $(SOURCES:%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean install uninstall test help

all: $(TARGETS)

$(BIN_DIR)/rafaelia_arch_probe: $(OBJ_DIR)/rafaelia_arch_probe.o | $(BIN_DIR)
	@echo "[LINK] $@"
	@$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	@echo "[CLEAN]"
	@rm -rf $(BIN_DIR) $(OBJ_DIR)

install: all
	@echo "[INSTALL] to $(DESTDIR)$(PREFIX)"
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@install -m 0755 $(BIN_DIR)/rafaelia_arch_probe $(DESTDIR)$(PREFIX)/bin/
	@mkdir -p $(DESTDIR)$(PREFIX)/share/doc/rafaelia
	@install -m 0644 README.md $(DESTDIR)$(PREFIX)/share/doc/rafaelia/
	@install -m 0644 LICENSE $(DESTDIR)$(PREFIX)/share/doc/rafaelia/

uninstall:
	@echo "[UNINSTALL] from $(DESTDIR)$(PREFIX)"
	@rm -f $(DESTDIR)$(PREFIX)/bin/rafaelia_arch_probe
	@rm -rf $(DESTDIR)$(PREFIX)/share/doc/rafaelia

test: all
	@echo "[TEST] rafaelia_arch_probe"
	@$(BIN_DIR)/rafaelia_arch_probe > /tmp/rafaelia_test.json
	@if [ -s /tmp/rafaelia_test.json ]; then \
		echo "[PASS] JSON output generated"; \
		cat /tmp/rafaelia_test.json; \
	else \
		echo "[FAIL] No JSON output"; \
		exit 1; \
	fi
	@rm -f /tmp/rafaelia_test.json

help:
	@echo "Rafaelia Module Build System"
	@echo ""
	@echo "Usage:"
	@echo "  make              Build all binaries"
	@echo "  make clean        Remove build artifacts"
	@echo "  make install      Install binaries and documentation"
	@echo "  make uninstall    Remove installed files"
	@echo "  make test         Run basic functionality tests"
	@echo ""
	@echo "Variables:"
	@echo "  CC                C compiler (default: cc)"
	@echo "  CFLAGS            C compiler flags"
	@echo "  LDFLAGS           Linker flags"
	@echo "  PREFIX            Installation prefix (default: /usr/local)"
	@echo "  DESTDIR           Installation root (default: empty)"
	@echo "  ANDROID_NDK       Path to Android NDK (for cross-compilation)"
	@echo "  NDK_ARCH          Android architecture (arm64, arm, x86_64, x86)"
	@echo "  NDK_API_LEVEL     Android API level (default: 21)"
	@echo ""
	@echo "Examples:"
	@echo "  make CC=clang                    Build with Clang"
	@echo "  make ANDROID_NDK=/path/to/ndk    Cross-compile for Android"
	@echo "  make PREFIX=/opt/rafaelia install Install to custom location"
