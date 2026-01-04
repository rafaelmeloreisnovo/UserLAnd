# RAFAELIA Checks & Validation Documentation

**Author:** @rafaelmeloreisnovo  
**RAFCODE-Φ Seal:** ⟨Φ⟩-CHECKS-DOCUMENTATION-v1.0  
**Last Updated:** 2026-01-04

This document describes the automated checks, validation tools, and governance mechanisms implemented in the UserLAnd repository.

## 📋 Table of Contents

1. [Overview](#overview)
2. [Workflow Validation](#workflow-validation)
3. [CI Matrix](#ci-matrix)
4. [RAFAELIA Tools](#rafaelia-tools)
5. [SBOM Generation](#sbom-generation)
6. [Signing and Verification](#signing-and-verification)
7. [Branch Protection](#branch-protection)
8. [Troubleshooting](#troubleshooting)

---

## 🎯 Overview

The RAFAELIA validation system provides:

- **Workflow Validation**: Line-by-line analysis of GitHub Actions workflows
- **Multi-Language CI**: Matrix builds for C, Assembly, Kotlin, Java, Python, and Shell
- **Ethica[8] Engine**: Ethical constraint checking (prevents infinite loops)
- **BitRAF64**: Compression/decompression utilities
- **RAFCODE-Φ**: Code signing and provenance tracking
- **SBOM**: Software Bill of Materials generation
- **Governance**: DCO, GPG signing, and code ownership

All tools are **prototypes** requiring review and testing before production use.

---

## 🔍 Workflow Validation

### Purpose

The workflow validator (`tools/check_workflows.py`) analyzes GitHub Actions workflow files for:

- YAML syntax errors
- Hardcoded secrets
- Command injection vulnerabilities
- Deprecated action versions
- Missing timeouts
- Best practices violations

### Running Locally

```bash
# Run the validator
python tools/check_workflows.py

# Output: workflow_findings.json
```

### CI Integration

The validation runs automatically via `.github/workflows/validate-workflows.yml`:

- **Triggers**: PRs and pushes affecting workflows
- **Output**: Findings artifact uploaded to GitHub
- **Summary**: Results displayed in job summary

### Findings Format

```json
[
  {
    "severity": "critical",
    "file": "workflow.yml",
    "line": 42,
    "message": "Potential hardcoded secret detected"
  }
]
```

**Severity Levels:**
- `critical`: Must fix before merge
- `error`: Should fix before merge
- `warning`: Review recommended
- `info`: Informational only

---

## 🔨 CI Matrix

### Purpose

Multi-language CI matrix (`.github/workflows/ci.yml`) validates:

- **C**: Compilation of C source files
- **Kotlin/Java**: Android Gradle builds and tests
- **Python**: Linting (flake8) and unit tests (pytest)
- **Shell**: shellcheck validation and syntax checks
- **Assembly**: File inventory (compilation requires specific toolchain)

### Running Locally

#### C Build
```bash
gcc -c your_file.c -o your_file.o
```

#### Kotlin/Java Build
```bash
./gradlew assembleDebug
./gradlew testDebugUnitTest
```

#### Python Lint
```bash
pip install flake8
flake8 your_file.py --max-line-length=120
```

#### Shell Check
```bash
shellcheck your_script.sh
bash -n your_script.sh  # Syntax check
```

### CI Matrix Jobs

| Job | Purpose | Timeout |
|-----|---------|---------|
| `c-build` | Compile C files | 15 min |
| `kotlin-java-build` | Build Android app | 20 min |
| `python-test` | Lint and test Python | 10 min |
| `shell-test` | Validate shell scripts | 10 min |
| `assembly-check` | Inventory ASM files | 5 min |
| `ci-summary` | Generate summary | 5 min |

---

## 🛠️ RAFAELIA Tools

### Ethica[8] Engine

**Purpose**: Detect infinite loops, unbounded recursion, and dangerous code patterns.

**Usage:**
```bash
python tools/rafaelia_ethica_engine.py <file_or_directory>
```

**Example:**
```bash
# Analyze a single file
python tools/rafaelia_ethica_engine.py script.py

# Analyze a directory
python tools/rafaelia_ethica_engine.py tools/
```

**Checks:**
- Infinite `while True` loops without breaks
- Unbounded recursion
- Dangerous functions (`eval`, `exec`)

**Eight Principles:**
1. No infinite loops
2. Bounded recursion
3. Resource limits
4. Time constraints
5. Memory bounds
6. I/O throttling
7. Safe concurrency
8. Graceful degradation

---

### BitRAF64 Compressor

**Purpose**: Compress data with zlib and encode as base64.

**Usage:**
```bash
# Compress a file
python tools/bitraf64_compressor.py input.txt output.txt.br64 9

# Compression level: 0-9 (default: 6)
```

**Example:**
```bash
python tools/bitraf64_compressor.py README.md README.md.br64
# Output: README.md.br64
```

---

### BitRAF64 Decompressor

**Purpose**: Decompress BitRAF64 files.

**Usage:**
```bash
# Decompress a file
python tools/bitraf64_decompressor.py input.txt.br64 output.txt
```

**Example:**
```bash
python tools/bitraf64_decompressor.py README.md.br64 README.md
# Output: README.md
```

---

### RAFCODE-Φ Encoder

**Purpose**: Generate cryptographic seals for code authentication.

**Usage:**
```bash
# Generate seal for a file
python tools/rafacode_encoder.py script.py MY-COMPONENT

# Output: script.rafcode.json
```

**Example:**
```bash
python tools/rafacode_encoder.py tools/check_workflows.py WORKFLOW-VALIDATOR
```

**Seal Format:**
```json
{
  "seal": "⟨Φ⟩-WORKFLOW-VALIDATOR-v1.0",
  "author": "@rafaelmeloreisnovo",
  "file": "check_workflows.py",
  "hash": {
    "algorithm": "SHA-256",
    "value": "abc123..."
  },
  "timestamp": "2026-01-04T00:00:00Z",
  "version": "1.0"
}
```

**Embedding in Code:**
```python
# Author: @rafaelmeloreisnovo
# RAFCODE-Φ Seal: ⟨Φ⟩-WORKFLOW-VALIDATOR-v1.0
```

---

## 📦 SBOM Generation

### Purpose

Generate Software Bill of Materials using [Syft](https://github.com/anchore/syft) and compute integrity hashes.

### Prerequisites

```bash
# Install Syft
curl -sSfL https://raw.githubusercontent.com/anchore/syft/main/install.sh | sh -s -- -b /usr/local/bin
```

### Usage

```bash
# Generate SBOM for current directory
./tools/generate_sbom.sh

# Generate SBOM for specific directory
./tools/generate_sbom.sh /path/to/project

# Specify format and output
./tools/generate_sbom.sh -f spdx -o sbom.spdx

# Generate SBOM for Docker image
./tools/generate_sbom.sh myimage:latest
```

### Formats

- `json` (default): JSON format
- `spdx`: SPDX 2.2 format
- `cyclonedx`: CycloneDX format

### Output Files

- `sbom.json`: SBOM data
- `sbom.sha256`: SHA-256 hash for integrity verification

### Verification

```bash
# Verify SBOM integrity
sha256sum -c sbom.sha256
# or
shasum -a 256 -c sbom.sha256
```

---

## 🔐 Signing and Verification

### GPG Commit Signing

**Setup:**
```bash
# Generate GPG key
gpg --full-generate-key

# List keys
gpg --list-secret-keys --keyid-format=long

# Configure Git
git config --global user.signingkey YOUR_KEY_ID
git config --global commit.gpgsign true
```

**Signing Commits:**
```bash
# Sign a commit
git commit -S -m "Your message"

# Verify signature
git log --show-signature -1
```

**Verify on GitHub:**
- Signed commits show "Verified" badge
- GitHub verifies GPG signature against your account

---

### DCO Sign-off

**Purpose**: Developer Certificate of Origin certifies your right to contribute.

**Usage:**
```bash
# Sign off commit
git commit -s -m "Your message"

# Adds: Signed-off-by: Your Name <email@example.com>
```

**Combine with GPG:**
```bash
git commit -S -s -m "Your message"
```

---

### File Hashing

**Generate hash:**
```bash
# SHA-256
sha256sum file.txt > file.txt.sha256

# Or using shasum
shasum -a 256 file.txt > file.txt.sha256
```

**Verify hash:**
```bash
sha256sum -c file.txt.sha256
# or
shasum -a 256 -c file.txt.sha256
```

---

## 🛡️ Branch Protection

### Recommended Settings

Enable these settings in GitHub repository settings:

#### Branch: `main` and `master`

**Protection Rules:**
- ✅ Require pull request reviews before merging
  - Required approving reviews: 1
  - Dismiss stale reviews when new commits are pushed
- ✅ Require status checks to pass before merging
  - Required checks:
    - `validate-workflows`
    - `ci-summary` (from CI matrix)
    - `CodeQL`
- ✅ Require conversation resolution before merging
- ✅ Require signed commits
- ✅ Require linear history
- ✅ Include administrators

**Additional Settings:**
- ✅ Allow force pushes: **Disabled**
- ✅ Allow deletions: **Disabled**

#### CODEOWNERS

The `CODEOWNERS` file automatically requests review from:
- `@rafaelmeloreisnovo` for all changes
- Special review for workflows, security files, and build configs

---

## ❓ Troubleshooting

### Workflow Validation Fails

**Issue**: Critical findings in workflow validation

**Solution:**
1. Review `workflow_findings.json`
2. Fix critical and error severity issues
3. Re-run validation locally
4. Push fixes and verify CI passes

---

### CI Matrix Job Fails

**Issue**: Specific language build fails

**Solutions:**

**C Build:**
```bash
# Check for syntax errors
gcc -fsyntax-only your_file.c
```

**Kotlin/Java:**
```bash
# Clean and rebuild
./gradlew clean assembleDebug
```

**Python:**
```bash
# Check specific issues
flake8 --show-source your_file.py
```

**Shell:**
```bash
# Check specific script
shellcheck -x your_script.sh
```

---

### Ethica[8] False Positives

**Issue**: Ethica[8] flags safe code as infinite loop

**Solution:**
- Review the flagged code
- Ensure loops have clear exit conditions
- Add explicit break or return statements
- Consider refactoring to make exit conditions more obvious

---

### SBOM Generation Fails

**Issue**: Syft not found or generation fails

**Solution:**
```bash
# Install Syft manually
curl -sSfL https://raw.githubusercontent.com/anchore/syft/main/install.sh | sh -s -- -b /usr/local/bin

# Verify installation
syft version

# Try generation again
./tools/generate_sbom.sh
```

---

### GPG Signing Issues

**Issue**: Commits not signed or verification fails

**Solution:**
```bash
# Check GPG configuration
git config --get user.signingkey

# List GPG keys
gpg --list-secret-keys

# Test GPG
echo "test" | gpg --clearsign

# Add GPG key to GitHub
gpg --armor --export YOUR_KEY_ID
# Then add to GitHub Settings > SSH and GPG keys
```

---

## 📚 Additional Resources

### Documentation
- `.github/CONTRIBUTING.md` - Contribution guidelines
- `.github/SECURITY.md` - Security policy
- `docs/TECHNICAL_DOCUMENTATION.md` - Technical details

### External Links
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Syft SBOM Tool](https://github.com/anchore/syft)
- [GPG Signature Verification](https://docs.github.com/en/authentication/managing-commit-signature-verification)
- [Developer Certificate of Origin](https://developercertificate.org/)

---

## 🤝 Support

For questions or issues:

1. **GitHub Issues**: Report bugs or request features
2. **GitHub Discussions**: Ask questions
3. **Security Issues**: Use `.github/SECURITY.md` guidelines

---

## 📝 Notes

- All tools in `tools/` are **prototypes**
- Review and test before production use
- Regular updates recommended for dependencies
- Security vulnerabilities should be reported privately

---

**Maintained by:** @rafaelmeloreisnovo  
**License:** MIT  
**Project:** UserLAnd - Linux on Android
