#!/bin/bash
# SBOM Generator Script
# Author: @rafaelmeloreisnovo
# RAFCODE-Φ: SBOM-GENERATOR-v1.0

set -euo pipefail

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_DIR="${ROOT_DIR}/build/sbom"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

echo_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

echo_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

echo_error() {
    echo -e "${RED}✗ $1${NC}" >&2
}

# Check if syft is available
check_syft() {
    if command -v syft &> /dev/null; then
        echo_success "syft found: $(syft version --output json 2>/dev/null | grep -o '"version":"[^"]*"' || echo 'installed')"
        return 0
    else
        echo_warning "syft not found"
        echo_info "Install syft: https://github.com/anchore/syft#installation"
        echo_info "  curl -sSfL https://raw.githubusercontent.com/anchore/syft/main/install.sh | sh -s -- -b /usr/local/bin"
        return 1
    fi
}

# Generate SBOM using syft
generate_sbom_syft() {
    local target="$1"
    local output_file="$2"
    local format="${3:-spdx-json}"
    
    echo_info "Generating SBOM for: ${target}"
    echo_info "Output format: ${format}"
    
    if syft "${target}" -o "${format}=${output_file}" 2>&1 | tee /tmp/syft.log; then
        echo_success "SBOM generated: ${output_file}"
        return 0
    else
        echo_error "Failed to generate SBOM"
        return 1
    fi
}

# Generate fallback SBOM (basic inventory)
generate_fallback_sbom() {
    local output_file="$1"
    
    echo_info "Generating fallback SBOM (basic inventory)"
    
    cat > "${output_file}" <<EOF
{
  "bomFormat": "CycloneDX",
  "specVersion": "1.4",
  "version": 1,
  "metadata": {
    "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
    "tools": [
      {
        "vendor": "rafaelmeloreisnovo",
        "name": "generate_sbom.sh",
        "version": "1.0.0"
      }
    ],
    "component": {
      "type": "application",
      "name": "UserLAnd",
      "version": "unknown"
    }
  },
  "components": []
}
EOF
    
    echo_warning "Generated minimal SBOM (install syft for complete analysis)"
    echo_success "Fallback SBOM: ${output_file}"
}

# Main function
main() {
    echo_info "🔨 SBOM Generator Script"
    echo_info "Root directory: ${ROOT_DIR}"
    
    # Parse arguments
    local target="${1:-${ROOT_DIR}}"
    local format="${2:-spdx-json}"
    local output_name="${3:-sbom}"
    
    # Create output directory
    mkdir -p "${OUTPUT_DIR}"
    
    # Determine output file extension
    local extension="json"
    case "${format}" in
        *json) extension="json" ;;
        *xml) extension="xml" ;;
        *text) extension="txt" ;;
    esac
    
    local output_file="${OUTPUT_DIR}/${output_name}.${extension}"
    
    # Check for syft and generate SBOM
    if check_syft; then
        if generate_sbom_syft "${target}" "${output_file}" "${format}"; then
            echo_success "SBOM generation completed successfully"
            echo_info "📄 Output: ${output_file}"
            
            # Display summary if JSON
            if [[ "${extension}" == "json" ]] && command -v jq &> /dev/null; then
                echo ""
                echo_info "📊 SBOM Summary:"
                jq -r '
                    if .artifacts then
                        "  Artifacts: \(.artifacts | length)"
                    elif .components then
                        "  Components: \(.components | length)"
                    else
                        "  Unable to determine component count"
                    end
                ' "${output_file}" 2>/dev/null || echo "  (summary not available)"
            fi
            
            return 0
        else
            echo_error "SBOM generation failed, creating fallback"
            generate_fallback_sbom "${output_file}"
            return 1
        fi
    else
        echo_warning "syft not available, creating fallback SBOM"
        generate_fallback_sbom "${output_file}"
        return 1
    fi
}

# Show usage if --help
if [[ "${1:-}" == "--help" ]] || [[ "${1:-}" == "-h" ]]; then
    cat <<EOF
Usage: $0 [TARGET] [FORMAT] [OUTPUT_NAME]

Generate Software Bill of Materials (SBOM) for the project.

Arguments:
  TARGET       Path to analyze (default: project root)
  FORMAT       Output format (default: spdx-json)
               Options: spdx-json, cyclonedx-json, syft-json, etc.
  OUTPUT_NAME  Output filename without extension (default: sbom)

Examples:
  $0
  $0 . spdx-json project-sbom
  $0 app/build/outputs/apk cyclonedx-json app-sbom

Requirements:
  - syft (recommended): https://github.com/anchore/syft
  - jq (optional, for summary display)

EOF
    exit 0
fi

# Run main
main "$@"
