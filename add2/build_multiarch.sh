#!/bin/bash
# RAFAELIA Multi-Architecture Build Script
# Auto-detects architecture and compiles with optimal settings
# Supports: x86_64, ARM, ARM64, RISC-V, MIPS, PowerPC, SPARC, s390x, and more

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== RAFAELIA Multi-Architecture Build System ===${NC}"
echo ""

# Detect architecture
detect_arch() {
    local arch=$(uname -m)
    echo -e "${YELLOW}Detected architecture: ${GREEN}$arch${NC}"
    
    case "$arch" in
        x86_64|amd64)
            echo "x86_64"
            ;;
        aarch64|arm64)
            echo "arm64"
            ;;
        armv7l|armhf)
            echo "arm32"
            ;;
        riscv64)
            echo "riscv"
            ;;
        mips|mips64)
            echo "mips"
            ;;
        ppc64|ppc64le|powerpc64)
            echo "powerpc"
            ;;
        sparc64)
            echo "sparc"
            ;;
        s390x)
            echo "s390x"
            ;;
        alpha)
            echo "alpha"
            ;;
        ia64)
            echo "ia64"
            ;;
        *)
            echo "unknown"
            ;;
    esac
}

# Get compiler
get_compiler() {
    if command -v gcc >/dev/null 2>&1; then
        echo "gcc"
    elif command -v clang >/dev/null 2>&1; then
        echo "clang"
    else
        echo -e "${RED}Error: No compiler found (gcc or clang required)${NC}"
        exit 1
    fi
}

# Detect compiler capabilities
detect_compiler_features() {
    local compiler=$1
    echo -e "${YELLOW}Detecting compiler features...${NC}"
    
    # Check C11 support
    if $compiler -std=c11 -xc - -o /dev/null >/dev/null 2>&1 <<< "int main(){return 0;}"; then
        echo -e "  ${GREEN}✓${NC} C11 support"
        echo "c11"
    else
        echo -e "  ${RED}✗${NC} C11 not supported"
        echo "c99"
    fi
}

# Architecture-specific compiler flags
get_arch_flags() {
    local arch=$1
    local flags=""
    
    case "$arch" in
        x86_64)
            flags="-march=x86-64 -mtune=generic"
            ;;
        arm64)
            flags="-march=armv8-a"
            ;;
        arm32)
            flags="-march=armv7-a -mfpu=neon"
            ;;
        riscv)
            flags="-march=rv64gc"
            ;;
        mips)
            flags="-march=mips64r2"
            ;;
        powerpc)
            flags="-mcpu=power8"
            ;;
        *)
            flags=""
            ;;
    esac
    
    echo "$flags"
}

# Main build function
build_rafaelia() {
    local source_file="$1"
    local output_file="$2"
    local compiler=$(get_compiler)
    local arch=$(detect_arch)
    local std_flag="-std=c11"
    local arch_flags=$(get_arch_flags "$arch")
    
    echo ""
    echo -e "${BLUE}Building RAFAELIA for $arch...${NC}"
    echo -e "  Compiler: ${GREEN}$compiler${NC}"
    echo -e "  Source: ${YELLOW}$source_file${NC}"
    echo -e "  Output: ${YELLOW}$output_file${NC}"
    
    # Build command
    local build_cmd="$compiler $std_flag -Wall -Wextra -O2 $arch_flags -o $output_file $source_file"
    
    echo ""
    echo -e "${YELLOW}Executing:${NC} $build_cmd"
    echo ""
    
    if $build_cmd 2>&1; then
        echo ""
        echo -e "${GREEN}✓ Build successful!${NC}"
        
        # Show binary info
        if command -v file >/dev/null 2>&1; then
            echo -e "${YELLOW}Binary info:${NC}"
            file "$output_file"
        fi
        
        # Show size
        if command -v du >/dev/null 2>&1; then
            local size=$(du -h "$output_file" | cut -f1)
            echo -e "${YELLOW}Binary size:${NC} $size"
        fi
        
        return 0
    else
        echo ""
        echo -e "${RED}✗ Build failed!${NC}"
        return 1
    fi
}

# Test function
test_rafaelia() {
    local binary="$1"
    
    echo ""
    echo -e "${BLUE}Testing $binary...${NC}"
    
    # Remove old output
    rm -f fractalMatrix.txt
    
    # Run binary
    if ./"$binary"; then
        echo -e "${GREEN}✓ Execution successful${NC}"
        
        # Check output
        if [ -f fractalMatrix.txt ]; then
            local lines=$(wc -l < fractalMatrix.txt)
            local hash=$(md5sum fractalMatrix.txt | cut -d' ' -f1)
            echo -e "  Lines: ${YELLOW}$lines${NC}"
            echo -e "  MD5: ${YELLOW}$hash${NC}"
            
            # Test determinism
            rm -f fractalMatrix.txt
            ./"$binary" >/dev/null 2>&1
            local hash2=$(md5sum fractalMatrix.txt | cut -d' ' -f1)
            
            if [ "$hash" = "$hash2" ]; then
                echo -e "  ${GREEN}✓ Deterministic output${NC}"
            else
                echo -e "  ${RED}✗ Non-deterministic output${NC}"
            fi
        else
            echo -e "${RED}✗ No output file generated${NC}"
            return 1
        fi
    else
        echo -e "${RED}✗ Execution failed${NC}"
        return 1
    fi
}

# Main execution
main() {
    local source_files=(
        "RAFAELIA_CORE_MATRIX_multiarch.c"
        "RAFAELIA_CORE_MATRIX_absolute.c"
        "RAFAELIA_CORE_MATRIX_lowlevel.c"
        "RAFAELIA_CORE_noincludes.c"
    )
    
    local arch=$(detect_arch)
    echo ""
    echo -e "${BLUE}Architecture: ${GREEN}$arch${NC}"
    
    # Build all available files
    for source in "${source_files[@]}"; do
        if [ -f "$source" ]; then
            local output="${source%.c}"
            echo ""
            echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
            
            if build_rafaelia "$source" "$output"; then
                if [ "$1" = "--test" ] || [ "$1" = "-t" ]; then
                    test_rafaelia "$output"
                fi
            fi
        fi
    done
    
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}Build complete!${NC}"
    echo ""
    echo -e "${YELLOW}Available binaries:${NC}"
    for source in "${source_files[@]}"; do
        local output="${source%.c}"
        if [ -f "$output" ]; then
            echo -e "  ${GREEN}✓${NC} $output"
        fi
    done
}

# Parse arguments
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "Usage: $0 [--test|-t]"
    echo ""
    echo "Options:"
    echo "  --test, -t    Run tests after building"
    echo "  --help, -h    Show this help message"
    echo ""
    echo "Supported architectures:"
    echo "  x86_64, ARM64, ARM32, RISC-V, MIPS, PowerPC, SPARC, s390x, and more"
    exit 0
fi

# Run main
main "$@"
