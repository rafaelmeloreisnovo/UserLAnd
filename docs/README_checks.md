# README: Workflow Checks and CI Validation

**Author:** @rafaelmeloreisnovo  
**RAFCODE-Φ seal:** PROTOTYPE-DRAFT-v0.1.0  
**Status:** Draft for review - requires maintainer approval

---

## 🔐 Important Notice

**This document describes PROTOTYPE/DRAFT implementations.** All workflows, checks, and validation tools require explicit approval from the repository maintainer before being enforced or activated.

**RAFCODE-Φ guarantees:**
- Authorship attribution: @rafaelmeloreisnovo
- No automatic execution of infinite loops
- Ethica[8] enforcement active
- All artifacts are prototypes/drafts

---

## Overview

This repository includes a suite of validation tools and CI workflows designed to ensure code quality, security, and compliance with repository policies. All tools are marked as **PROTOTYPE-DRAFT** status.

### Key Components

1. **Workflow Validator** - Validates GitHub Actions workflows
2. **Ethica[8] Engine** - Prevents unsafe code execution
3. **CI Matrix** - Multi-platform testing and validation
4. **BITRAF64 Tools** - Compression with metadata
5. **RAFCODE Encoder** - Cryptographic seals and provenance
6. **SBOM Generator** - Software Bill of Materials

---

## Validation Tools

### 1. Workflow Validator

**Location:** `tools/check_workflows.py`

Validates GitHub Actions workflow files for:
- YAML syntax correctness
- Required fields (name, on, jobs)
- Security best practices
- Script injection vulnerabilities
- Action version pinning

**Usage:**
```bash
# Validate all workflows
python tools/check_workflows.py

# From repository root
python tools/check_workflows.py
```

**Output:**
```
======================================================================
RAFAELIA Workflow Validator - Prototype v0.1.0
Author: @rafaelmeloreisnovo | RAFCODE-Φ seal
======================================================================
🔍 Validating 5 workflow file(s)...
✅ android-debug.yml: VALID
✅ ci.yml: VALID
✅ validate-workflows.yml: VALID
======================================================================
✅ All workflows validated successfully
======================================================================
```

### 2. Ethica[8] Engine

**Location:** `tools/rafaelia_ethica_engine.py`

Analyzes Python code for Ethica[8] violations:
- Infinite loops (`while True`)
- Unbounded recursion
- Unsafe system calls
- Resource limit violations

**Usage:**
```bash
# Check a Python file
python tools/rafaelia_ethica_engine.py <file.py>

# Example
python tools/rafaelia_ethica_engine.py tools/check_workflows.py
```

**Output:**
```
======================================================================
RAFAELIA Ethica[8] Engine - Prototype v0.1.0
Author: @rafaelmeloreisnovo | RAFCODE-Φ seal
======================================================================
✅ tools/check_workflows.py: No Ethica[8] violations detected
======================================================================
```

### 3. BITRAF64 Compressor

**Location:** `tools/bitraf64_compressor.py`

Compresses data with RAFCODE-Φ metadata embedding.

**Usage:**
```bash
# Compress a file
python tools/bitraf64_compressor.py <input> <output>

# Example
python tools/bitraf64_compressor.py data.txt data.bitraf64
```

**Features:**
- zlib compression (level 9)
- Base64 encoding
- Metadata embedding
- RAFCODE-Φ seal
- Authorship preservation

### 4. BITRAF64 Decompressor

**Location:** `tools/bitraf64_decompressor.py`

Decompresses BITRAF64 encoded files.

**Usage:**
```bash
# Decompress a file
python tools/bitraf64_decompressor.py <input> <output>

# Example
python tools/bitraf64_decompressor.py data.bitraf64 data.txt
```

**Features:**
- RAFCODE-Φ seal validation
- Metadata extraction
- Integrity verification
- Authorship verification

### 5. RAFCODE Encoder

**Location:** `tools/rafacode_encoder.py`

Generates RAFCODE-Φ seals and metadata for files.

**Usage:**
```bash
# Generate seal for a file
python tools/rafacode_encoder.py <file_or_directory>

# Example - single file
python tools/rafacode_encoder.py app/build.gradle

# Example - directory
python tools/rafacode_encoder.py tools/
```

**Output:**
```json
{
  "filename": "check_workflows.py",
  "path": "tools/check_workflows.py",
  "size": 4344,
  "content_hash": "abc123...",
  "author": "@rafaelmeloreisnovo",
  "seal": "RAFCODE-Φ:{...}"
}
```

### 6. SBOM Generator

**Location:** `tools/generate_sbom.sh`

Generates Software Bill of Materials (SBOM) in CycloneDX format.

**Usage:**
```bash
# Generate SBOM
./tools/generate_sbom.sh [output_file]

# Default output: sbom.json
./tools/generate_sbom.sh

# Custom output
./tools/generate_sbom.sh sbom-custom.json
```

**Output Format:**
- CycloneDX 1.4 specification
- Component metadata
- Dependency information
- RAFCODE-Φ seal

---

## GitHub Actions Workflows

### Workflow: validate-workflows

**File:** `.github/workflows/validate-workflows.yml`

**Triggers:**
- Push to `master` or `rafaelia/**` branches (workflow files changed)
- Pull requests (workflow files changed)
- Manual dispatch

**Jobs:**
1. Checkout repository
2. Set up Python 3.11
3. Install dependencies (pyyaml)
4. Run Ethica[8] validation on tools
5. Validate all workflow files
6. Generate SBOM (on push to master)
7. Upload SBOM artifact

**Status Check:** `validate-workflows`

### Workflow: ci (CI Matrix)

**File:** `.github/workflows/ci.yml`

**Triggers:**
- Push to `master` or `rafaelia/**` branches
- Pull requests to `master`
- Manual dispatch

**Jobs:**

#### 1. matrix-test
Tests across multiple configurations:
- OS: ubuntu-latest, macos-latest
- Java: 17, 21

**Steps:**
1. Checkout with submodules
2. Set up Python 3.11
3. Install Python dependencies
4. Run Ethica[8] checks
5. Set up Java
6. Setup Android SDK (Ubuntu only)
7. Gradle cache
8. Build debug APK (Ubuntu only)
9. Run tests (Ubuntu + Java 17 only)
10. Upload test reports
11. Upload APK artifacts

#### 2. security-checks
Security validation:
1. Checkout repository
2. Set up Python
3. Install dependencies
4. Validate workflows
5. Generate SBOM
6. Upload SBOM artifact

#### 3. status-check
Aggregates results from all jobs and reports overall CI status.

**Status Check:** `ci`

---

## Status Checks (Proposed)

**⚠️ PROTOTYPE - Requires maintainer approval:**

### Required Checks Before Merge

1. **validate-workflows** ✅
   - All workflow files pass validation
   - No security issues detected
   - Ethica[8] compliance verified

2. **ci** ✅
   - Build succeeds on all platforms
   - Tests pass
   - Security checks pass

3. **Code Review** 👤
   - At least 1 approving review from maintainer
   - All comments resolved

4. **Signed Commits** ✍️
   - All commits signed with GPG/SSH
   - DCO (Developer Certificate of Origin) compliance

### Branch Protection Settings (Proposed)

```yaml
# Proposed settings for protected branches (master)
protection:
  required_status_checks:
    strict: true
    checks:
      - validate-workflows
      - ci
  required_pull_request_reviews:
    required_approving_review_count: 1
    dismiss_stale_reviews: true
    require_code_owner_reviews: true
  required_signatures: true
  enforce_admins: false
  restrictions: null
```

---

## Using the Tools Locally

### Prerequisites

```bash
# Install Python 3.11+
python3 --version

# Install dependencies
pip install pyyaml
```

### Running Validation Locally

```bash
# Navigate to repository root
cd /path/to/UserLAnd

# Validate workflows
python tools/check_workflows.py

# Check Python files for Ethica[8] violations
python tools/rafaelia_ethica_engine.py tools/check_workflows.py

# Generate SBOM
./tools/generate_sbom.sh

# Test compression/decompression
echo "Hello RAFAELIA" > test.txt
python tools/bitraf64_compressor.py test.txt test.bitraf64
python tools/bitraf64_decompressor.py test.bitraf64 test_restored.txt

# Generate RAFCODE seal
python tools/rafacode_encoder.py tools/
```

### Pre-commit Checks

Consider adding these to your pre-commit hook:

```bash
#!/bin/bash
# .git/hooks/pre-commit

set -e

echo "🔍 Running pre-commit checks..."

# Validate workflows if changed
if git diff --cached --name-only | grep -q "^.github/workflows/"; then
    echo "📋 Validating workflows..."
    python tools/check_workflows.py
fi

# Check Python files if changed
for file in $(git diff --cached --name-only | grep "\.py$"); do
    echo "🔐 Checking $file for Ethica[8] violations..."
    python tools/rafaelia_ethica_engine.py "$file"
done

echo "✅ Pre-commit checks passed"
```

---

## Troubleshooting

### Workflow Validation Fails

**Problem:** Workflow validation reports errors

**Solutions:**
1. Check YAML syntax with `yamllint`
2. Ensure all required fields are present
3. Pin action versions: `actions/checkout@v4` not `actions/checkout`
4. Avoid script injection with `${{ github.event }}`

### Ethica[8] Violations

**Problem:** Code fails Ethica[8] checks

**Solutions:**
1. Replace `while True:` with bounded loops
2. Add termination conditions
3. Implement timeout mechanisms
4. Use resource limits

### CI Build Fails

**Problem:** CI matrix builds fail

**Solutions:**
1. Check Java version compatibility
2. Verify Android SDK setup
3. Review Gradle cache issues
4. Check platform-specific issues

### SBOM Generation Fails

**Problem:** SBOM generation fails

**Solutions:**
1. Ensure `generate_sbom.sh` is executable: `chmod +x tools/generate_sbom.sh`
2. Check bash version: `bash --version`
3. Verify repository structure
4. Check file permissions

---

## Integration with Existing Workflows

These validation tools are designed to complement, not replace, existing workflows:

### Existing Workflows
- `android-debug.yml` - Debug build
- `android-release.yml` - Release build  
- `userland-android-ci.yml` - Android CI
- `rafaelia-core-ci.yml` - RAFAELIA core CI
- `codeql-analysis.yml` - CodeQL security

### New Workflows (Prototype)
- `validate-workflows.yml` - Workflow validation
- `ci.yml` - CI matrix testing

All workflows can run in parallel without conflicts.

---

## Development Workflow

### Making Changes

1. **Create feature branch**
   ```bash
   git checkout -b feature/my-change
   ```

2. **Make changes**
   - Edit code, workflows, or documentation

3. **Run local validation**
   ```bash
   python tools/check_workflows.py
   python tools/rafaelia_ethica_engine.py <changed_files>
   ```

4. **Commit changes** (signed)
   ```bash
   git commit -S -m "feat: add new feature"
   ```

5. **Push and create PR**
   ```bash
   git push origin feature/my-change
   # Create PR on GitHub
   ```

6. **Wait for CI**
   - `validate-workflows` must pass
   - `ci` must pass
   - Code review required

7. **Merge** (after approval)
   - Maintainer reviews and approves
   - Squash and merge to master

---

## Future Enhancements

**Planned improvements (pending approval):**

- [ ] Automated dependency vulnerability scanning
- [ ] Integration with CodeQL for deeper security analysis
- [ ] Automated SBOM generation on releases
- [ ] Enhanced Ethica[8] rules (recursion depth, memory limits)
- [ ] BITRAF64 streaming compression
- [ ] RAFCODE-Φ seal verification service
- [ ] Pre-commit hook templates
- [ ] CI/CD dashboard integration
- [ ] Automated security advisories
- [ ] Supply chain attestation

---

## References

### Documentation
- [GitHub Actions Workflow Syntax](https://docs.github.com/en/actions/reference/workflow-syntax-for-github-actions)
- [CycloneDX SBOM Specification](https://cyclonedx.org/)
- [DCO (Developer Certificate of Origin)](https://developercertificate.org/)

### Repository Files
- `.github/CONTRIBUTING.md` - Contribution guidelines
- `.github/SECURITY.md` - Security policy
- `.github/pull_request_template.md` - PR template
- `CODEOWNERS` - Code ownership

### RAFAELIA Components
- `RAFAELIA_ROADMAP.md` - Project roadmap
- `RAFAELIA_SUMMARY.md` - Component summary
- `docs/TECHNICAL_DOCUMENTATION.md` - Technical docs

---

## Support

For questions or issues with validation tools:

1. Check this documentation
2. Review tool output and error messages
3. Test locally before pushing
4. Open an issue with detailed information
5. Contact maintainers for urgent matters

---

**RAFCODE-Φ seal:** @rafaelmeloreisnovo  
**Version:** 0.1.0  
**Status:** PROTOTYPE-DRAFT  
**Last Updated:** 2026-01-04

**DO NOT MERGE or ENFORCE** without explicit maintainer approval.  
All artifacts preserve authorship and require human review before activation.

---

## License

All validation tools and workflows are subject to the same license as the UserLAnd project. See `LICENSE` file in repository root.

## Acknowledgments

- UserLAnd maintainers and contributors
- RAFAELIA project framework
- Open source security tools and practices

---

*This document is part of the RAFAELIA initiative for enhanced code quality,  
security, and governance in the UserLAnd project.*
