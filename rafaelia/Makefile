# Rafaelia Module Makefile
# Copyright (c) 2024 Rafael Melo Reis
# Licensed under RAFAELIA MODULE LICENSE
#
# ==============================================================================
# Cross-Architecture Build Configuration
# ==============================================================================
# This Makefile supports building for multiple architectures:
# - Native compilation (Linux, macOS, BSD)
# - Cross-compilation for Android via NDK
#
# Supported Android architectures: arm64-v8a, armeabi-v7a, x86_64, x86
# ==============================================================================

.POSIX:
.SUFFIXES:

# Compiler and flags
CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -pedantic -fvisibility=hidden
LDFLAGS = 
PREFIX = /usr/local
DESTDIR =

# Include the compatibility header directory
INCLUDE_DIRS = -I../include

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

# ==============================================================================
# Architecture Detection
# ==============================================================================

ifeq ($(UNAME_M),x86_64)
	HOST_ARCH = x86_64
	CFLAGS += -DRAF_HOST_X86_64=1
else ifeq ($(UNAME_M),aarch64)
	HOST_ARCH = arm64
	CFLAGS += -DRAF_HOST_ARM64=1
else ifeq ($(UNAME_M),arm64)
	HOST_ARCH = arm64
	CFLAGS += -DRAF_HOST_ARM64=1
else ifeq ($(UNAME_M),armv7l)
	HOST_ARCH = arm
	CFLAGS += -DRAF_HOST_ARM32=1
else
	HOST_ARCH = $(UNAME_M)
endif

# ==============================================================================
# Android NDK Support
# ==============================================================================

ifdef ANDROID_NDK
	NDK_HOST_TAG = linux-x86_64
	ifeq ($(UNAME_S),Darwin)
		NDK_HOST_TAG = darwin-x86_64
	endif
	
	NDK_API_LEVEL ?= 21
	NDK_ARCH ?= arm64
	
	ifeq ($(NDK_ARCH),arm64)
		NDK_TARGET = aarch64-linux-android$(NDK_API_LEVEL)
		ARCH_CFLAGS = -march=armv8-a
		ANDROID_ABI = arm64-v8a
	else ifeq ($(NDK_ARCH),arm)
		NDK_TARGET = armv7a-linux-androideabi$(NDK_API_LEVEL)
		ARCH_CFLAGS = -march=armv7-a -mthumb -mfpu=neon
		ANDROID_ABI = armeabi-v7a
	else ifeq ($(NDK_ARCH),x86_64)
		NDK_TARGET = x86_64-linux-android$(NDK_API_LEVEL)
		ARCH_CFLAGS = -msse4.2
		ANDROID_ABI = x86_64
	else ifeq ($(NDK_ARCH),x86)
		NDK_TARGET = i686-linux-android$(NDK_API_LEVEL)
		ARCH_CFLAGS = -msse2
		ANDROID_ABI = x86
	endif
	
	CC = $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(NDK_HOST_TAG)/bin/clang
	CFLAGS += --target=$(NDK_TARGET) $(ARCH_CFLAGS)
	
	# Android 15 (API 35) 16KB page alignment
	ifeq ($(shell test $(NDK_API_LEVEL) -ge 35 && echo yes),yes)
		LDFLAGS += -Wl,-z,max-page-size=16384
		CFLAGS += -DRAF_ANDROID_15_COMPAT=1
	endif
	
	# Security hardening
	LDFLAGS += -Wl,-z,relro -Wl,-z,now
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
	@$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

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
	@echo "=============================================="
	@echo ""
	@echo "Usage:"
	@echo "  make              Build all binaries"
	@echo "  make clean        Remove build artifacts"
	@echo "  make install      Install binaries and documentation"
	@echo "  make uninstall    Remove installed files"
	@echo "  make test         Run basic functionality tests"
	@echo "  make all-archs    Build for all Android architectures"
	@echo ""
	@echo "Variables:"
	@echo "  CC                C compiler (default: cc)"
	@echo "  CFLAGS            C compiler flags"
	@echo "  LDFLAGS           Linker flags"
	@echo "  PREFIX            Installation prefix (default: /usr/local)"
	@echo "  DESTDIR           Installation root (default: empty)"
	@echo "  ANDROID_NDK       Path to Android NDK (for cross-compilation)"
	@echo "  NDK_ARCH          Android architecture (arm64, arm, x86_64, x86)"
	@echo "  NDK_API_LEVEL     Android API level (default: 21, max: 35)"
	@echo ""
	@echo "Supported Android Architectures:"
	@echo "  arm64    (arm64-v8a)     - 64-bit ARM (modern devices)"
	@echo "  arm      (armeabi-v7a)   - 32-bit ARM (older devices)"
	@echo "  x86_64                   - 64-bit Intel (emulators)"
	@echo "  x86                      - 32-bit Intel (legacy)"
	@echo ""
	@echo "Examples:"
	@echo "  make CC=clang                                Build with Clang"
	@echo "  make ANDROID_NDK=/path/to/ndk NDK_ARCH=arm64 Cross-compile for ARM64"
	@echo "  make ANDROID_NDK=/path/to/ndk NDK_API_LEVEL=35 Android 15 target"
	@echo "  make PREFIX=/opt/rafaelia install            Install to custom location"
	@echo ""
	@echo "Multi-Architecture Build:"
	@echo "  for arch in arm64 arm x86_64 x86; do \\"
	@echo "    make clean && make ANDROID_NDK=/path/to/ndk NDK_ARCH=\$$arch; \\"
	@echo "    mkdir -p build/\$$arch && cp bin/* build/\$$arch/; \\"
	@echo "  done"

# Build for all Android architectures (requires ANDROID_NDK to be set)
.PHONY: all-archs
all-archs:
ifdef ANDROID_NDK
	@echo "Building for all Android architectures..."
	@for arch in arm64 arm x86_64 x86; do \
		echo "Building for $$arch..."; \
		$(MAKE) clean; \
		$(MAKE) NDK_ARCH=$$arch; \
		mkdir -p build/$$arch; \
		cp $(BIN_DIR)/* build/$$arch/; \
	done
	@echo "All architectures built successfully!"
else
	@echo "Error: ANDROID_NDK not set. Please set ANDROID_NDK to your NDK path."
	@exit 1
endif
