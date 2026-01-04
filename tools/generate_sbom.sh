#!/usr/bin/env bash
# RAFAELIA SBOM Generator - Prototype/Draft
# Author: @rafaelmeloreisnovo
# RAFCODE-Φ seal: PROTOTYPE-DRAFT-v0.1.0
# License: See repository LICENSE
#
# Generates Software Bill of Materials (SBOM) for the repository.
# IMPORTANT: This is a prototype/draft implementation.

set -euo pipefail

echo "======================================================================"
echo "RAFAELIA SBOM Generator - Prototype v0.1.0"
echo "Author: @rafaelmeloreisnovo | RAFCODE-Φ seal"
echo "======================================================================"

# Find repository root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_ROOT}"

OUTPUT_FILE="${1:-sbom.json}"

echo "📦 Generating SBOM for repository..."
echo "   Root: ${REPO_ROOT}"
echo "   Output: ${OUTPUT_FILE}"

# Create SBOM structure
cat > "${OUTPUT_FILE}" << EOF
{
  "bomFormat": "CycloneDX",
  "specVersion": "1.4",
  "version": 1,
  "metadata": {
    "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
    "tools": [
      {
        "vendor": "RAFAELIA",
        "name": "generate_sbom.sh",
        "version": "0.1.0",
        "author": "@rafaelmeloreisnovo"
      }
    ],
    "component": {
      "type": "application",
      "name": "UserLAnd",
      "version": "draft",
      "description": "Android app for running Linux distributions"
    }
  },
  "components": [
EOF

# Add Gradle dependencies if available
if [ -f "app/build.gradle" ]; then
  echo "   📋 Scanning Gradle dependencies..."
  cat >> "${OUTPUT_FILE}" << 'COMPONENT_EOF'
    {
      "type": "library",
      "name": "gradle-dependencies",
      "description": "Dependencies from app/build.gradle"
    }
COMPONENT_EOF
fi

# Close components array
cat >> "${OUTPUT_FILE}" << EOF
  ],
  "rafcode": {
    "seal": "RAFCODE-Φ",
    "author": "@rafaelmeloreisnovo",
    "status": "PROTOTYPE-DRAFT",
    "note": "This SBOM is a prototype. Complete dependency scanning requires build-time analysis."
  }
}
EOF

echo "✅ SBOM generated: ${OUTPUT_FILE}"
echo "======================================================================"
