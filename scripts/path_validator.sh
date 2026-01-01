#!/usr/bin/env bash
# Path validation and normalization helper
# Prevents path doubling issues in build scripts
set -euo pipefail

# Common directory names that might legitimately repeat at multiple levels
COMMON_REPEAT_SEGMENTS=(
    bin lib usr opt etc var tmp home root
    src build test tests dist out
    node_modules vendor
)

is_common_repeat_segment() {
    local segment="$1"
    local candidate
    for candidate in "${COMMON_REPEAT_SEGMENTS[@]}"; do
        if [[ "$segment" == "$candidate" ]]; then
            return 0
        fi
    done
    return 1
}

# Function to normalize a path by removing duplicate non-common path segments
# Usage: normalize_path "/path/to/UserLAnd/UserLAnd/file"
# Output: "/path/to/UserLAnd/file"
normalize_path() {
    local input_path="$1"

    # Convert to absolute path if relative (without requiring the path to exist)
    if [[ ! "$input_path" = /* ]]; then
        input_path="$(pwd)/$input_path"
    fi
    
    # Split path into components
    local -a components
    IFS='/' read -ra components <<< "$input_path"
    
    # Remove duplicate consecutive segments (except common repeated names)
    local -a normalized
    local prev=""
    for comp in "${components[@]}"; do
        if [[ -z "$comp" ]]; then
            continue  # Skip empty components
        fi
        
        # Check for duplicate segment
        if [[ "$comp" != "$prev" ]] || [[ -z "$prev" ]]; then
            normalized+=("$comp")
        elif is_common_repeat_segment "$comp"; then
            normalized+=("$comp")
        fi
        prev="$comp"
    done
    
    # Reconstruct path
    local result="/"
    for comp in "${normalized[@]}"; do
        if [[ "$result" == "/" ]]; then
            result="/${comp}"
        else
            result="${result}/${comp}"
        fi
    done
    
    echo "$result"
}

# Function to check if a path contains doubled segments
# Returns 0 if doubled, 1 if not
# Only considers it doubled if the same non-common segment appears consecutively
#
# Common directory names that might legitimately repeat at multiple levels:
# - bin, lib, usr, opt, etc, var, tmp, home, root (system directories)
# - src, build, test, tests, dist, out (build artifacts and source organization)
# - node_modules, vendor (dependency directories that might nest)
has_doubled_path() {
    local input_path="$1"
    
    # Split path into components
    local -a components
    IFS='/' read -ra components <<< "$input_path"
    
    # Check for consecutive duplicate segments
    # But exclude common directory names that might legitimately repeat
    local prev=""
    for comp in "${components[@]}"; do
        if [[ -z "$comp" ]]; then
            continue
        fi
        
        # Only flag as doubled if it's an exact consecutive duplicate
        # AND it's not a common name that might legitimately appear at multiple levels
        if [[ "$comp" == "$prev" ]]; then
            if ! is_common_repeat_segment "$comp"; then
                # Any other repeated segment is likely an error
                return 0
            fi
        fi
        prev="$comp"
    done
    
    return 1  # No problematic doubled segments
}

# Function to safely join path components
# Usage: safe_join_path "/base/path" "relative/component"
# Output: "/base/path/relative/component"
safe_join_path() {
    local base="$1"
    local component="$2"
    
    # Remove trailing slash from base
    base="${base%/}"
    
    # Remove leading slash from component
    component="${component#/}"
    
    # Construct joined path
    local joined="$base/$component"
    
    # Normalize to remove any potential doubles
    normalize_path "$joined"
}

# Main CLI interface
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    case "${1:-}" in
        normalize)
            if [[ -z "${2:-}" ]]; then
                echo "Usage: $0 normalize <path>" >&2
                exit 1
            fi
            normalize_path "$2"
            ;;
        check)
            if [[ -z "${2:-}" ]]; then
                echo "Usage: $0 check <path>" >&2
                exit 1
            fi
            if has_doubled_path "$2"; then
                echo "WARNING: Path has doubled segments: $2" >&2
                exit 1
            else
                echo "OK: Path is properly formed: $2"
                exit 0
            fi
            ;;
        join)
            if [[ -z "${2:-}" ]] || [[ -z "${3:-}" ]]; then
                echo "Usage: $0 join <base> <component>" >&2
                exit 1
            fi
            safe_join_path "$2" "$3"
            ;;
        *)
            echo "Usage: $0 {normalize|check|join} <args>"
            echo ""
            echo "Commands:"
            echo "  normalize <path>        Normalize path by removing duplicate segments"
            echo "  check <path>            Check if path has doubled segments"
            echo "  join <base> <component> Safely join path components"
            echo ""
            echo "Examples:"
            echo "  $0 normalize /path/to/UserLAnd/UserLAnd/file"
            echo "  $0 check /base/path/path/file"
            echo "  $0 join /base/path relative/file"
            exit 1
            ;;
    esac
fi
