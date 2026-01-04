#!/bin/bash
#
# SBOM Generator - Software Bill of Materials
# Author: @rafaelmeloreisnovo
# RAFCODE-Φ Seal: ⟨Φ⟩-SBOM-GENERATOR-v1.0
#
# Generates SBOM using Syft and computes integrity hashes
# This is a PROTOTYPE script. Production use requires additional validation.

set -euo pipefail

# Configuration
SBOM_FORMAT="${SBOM_FORMAT:-json}"
SBOM_OUTPUT="${SBOM_OUTPUT:-sbom.json}"
HASH_OUTPUT="${HASH_OUTPUT:-sbom.sha256}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}ℹ️  $*${NC}"
}

log_success() {
    echo -e "${GREEN}✓ $*${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠️  $*${NC}"
}

log_error() {
    echo -e "${RED}❌ $*${NC}" >&2
}

check_dependencies() {
    log_info "Checking dependencies..."
    
    # Check for syft
    if ! command -v syft &> /dev/null; then
        log_warning "Syft not found. Installing..."
        
        # Try to install syft
        if command -v curl &> /dev/null; then
            log_info "Installing Syft via curl..."
            curl -sSfL https://raw.githubusercontent.com/anchore/syft/main/install.sh | sh -s -- -b /usr/local/bin
        else
            log_error "curl not found. Please install syft manually:"
            log_error "  https://github.com/anchore/syft#installation"
            return 1
        fi
    fi
    
    log_success "Dependencies OK"
}

generate_sbom() {
    local target="${1:-.}"
    
    log_info "Generating SBOM for: $target"
    
    # Run syft
    if syft "$target" -o "$SBOM_FORMAT" > "$SBOM_OUTPUT"; then
        log_success "SBOM generated: $SBOM_OUTPUT"
    else
        log_error "Failed to generate SBOM"
        return 1
    fi
    
    # Get file size
    local size
    size=$(stat -f%z "$SBOM_OUTPUT" 2>/dev/null || stat -c%s "$SBOM_OUTPUT" 2>/dev/null || echo "unknown")
    log_info "SBOM size: $size bytes"
}

generate_hash() {
    log_info "Generating integrity hash..."
    
    if [ ! -f "$SBOM_OUTPUT" ]; then
        log_error "SBOM file not found: $SBOM_OUTPUT"
        return 1
    fi
    
    # Generate SHA-256 hash
    if command -v sha256sum &> /dev/null; then
        sha256sum "$SBOM_OUTPUT" > "$HASH_OUTPUT"
    elif command -v shasum &> /dev/null; then
        shasum -a 256 "$SBOM_OUTPUT" > "$HASH_OUTPUT"
    else
        log_error "No SHA-256 utility found (sha256sum or shasum)"
        return 1
    fi
    
    log_success "Hash generated: $HASH_OUTPUT"
    
    # Display hash
    local hash_value
    hash_value=$(cut -d' ' -f1 "$HASH_OUTPUT")
    log_info "SHA-256: $hash_value"
}

display_summary() {
    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📊 SBOM GENERATION SUMMARY"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    if [ -f "$SBOM_OUTPUT" ]; then
        echo "✓ SBOM File:      $SBOM_OUTPUT"
        
        # Count packages if JSON format
        if [ "$SBOM_FORMAT" = "json" ] && command -v jq &> /dev/null; then
            local pkg_count
            pkg_count=$(jq '.artifacts | length' "$SBOM_OUTPUT" 2>/dev/null || echo "unknown")
            echo "  Packages:       $pkg_count"
        fi
    fi
    
    if [ -f "$HASH_OUTPUT" ]; then
        echo "✓ Hash File:      $HASH_OUTPUT"
        cat "$HASH_OUTPUT" | while read -r line; do
            echo "  Hash:           ${line%% *}"
        done
    fi
    
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    
    log_info "To verify SBOM integrity later, run:"
    if command -v sha256sum &> /dev/null; then
        echo "  sha256sum -c $HASH_OUTPUT"
    elif command -v shasum &> /dev/null; then
        echo "  shasum -a 256 -c $HASH_OUTPUT"
    fi
}

usage() {
    cat << EOF
SBOM Generator - Software Bill of Materials

Usage: $0 [OPTIONS] [TARGET]

Arguments:
  TARGET              Directory or image to scan (default: current directory)

Options:
  -f, --format FORMAT SBOM format: json, spdx, cyclonedx (default: json)
  -o, --output FILE   Output file (default: sbom.json)
  -h, --help          Show this help message

Environment Variables:
  SBOM_FORMAT         Default SBOM format
  SBOM_OUTPUT         Default output file
  HASH_OUTPUT         Default hash output file

Examples:
  # Generate SBOM for current directory
  $0

  # Generate SBOM for specific directory
  $0 /path/to/project

  # Generate SBOM in SPDX format
  $0 -f spdx -o sbom.spdx

  # Generate SBOM for Docker image
  $0 myimage:latest

For more information:
  https://github.com/anchore/syft
EOF
}

main() {
    local target="."
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -f|--format)
                SBOM_FORMAT="$2"
                shift 2
                ;;
            -o|--output)
                SBOM_OUTPUT="$2"
                HASH_OUTPUT="${2}.sha256"
                shift 2
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            -*)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
            *)
                target="$1"
                shift
                ;;
        esac
    done
    
    echo "🔍 RAFAELIA SBOM Generator"
    echo ""
    
    # Check dependencies
    if ! check_dependencies; then
        log_error "Dependency check failed"
        exit 1
    fi
    
    # Generate SBOM
    if ! generate_sbom "$target"; then
        log_error "SBOM generation failed"
        exit 1
    fi
    
    # Generate hash
    if ! generate_hash; then
        log_error "Hash generation failed"
        exit 1
    fi
    
    # Display summary
    display_summary
    
    log_success "SBOM generation complete!"
    exit 0
}

main "$@"
