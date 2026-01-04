# Workflow Checks and Validation Documentation
<!-- Author: @rafaelmeloreisnovo -->
<!-- RAFCODE-Φ: CHECKS-DOCUMENTATION-v1.0 -->

## 🔍 Overview

This documentation covers the validation tools, CI/CD checks, and governance mechanisms implemented in the UserLAnd repository.

## 🛠️ Validation Tools

### 1. Workflow Validator (`tools/check_workflows.py`)

**Purpose**: Performs line-by-line analysis of GitHub Actions workflow files to detect security issues, best practice violations, and potential problems.

#### Installation

```bash
# Install dependencies
pip install pyyaml
```

#### Usage

```bash
# Validate all workflows in default directory
python tools/check_workflows.py

# Specify custom workflows directory
python tools/check_workflows.py --workflows-dir .github/workflows

# Custom output file
python tools/check_workflows.py --output my-report.json
```

#### What It Checks

- ✅ YAML syntax validity
- ✅ Required workflow fields (name, jobs, runs-on, steps)
- ✅ Secret exposure patterns
- ✅ Unsigned container images
- ✅ Shell injection risks
- ✅ Best practices compliance

#### Output

Generates a JSON report with:
- File-by-file analysis
- Issues (errors), warnings, and info messages
- Line numbers for each finding
- Summary statistics

#### Example Output

```json
{
  "timestamp": "...",
  "total_files": 5,
  "results": [...],
  "summary": {
    "total_errors": 0,
    "total_warnings": 3,
    "total_info": 5
  }
}
```

### 2. Bitraf64 Compressor/Decompressor

**Purpose**: Prototype compression/decompression system with integrity markers.

#### Installation

```bash
# No additional dependencies required (uses stdlib)
```

#### Usage

```bash
# Compress a file
python tools/bitraf64_compressor.py input.txt output.b64 --level 9

# Decompress a file
python tools/bitraf64_decompressor.py output.b64 recovered.txt
```

#### Features

- DEFLATE compression (zlib)
- Base64 encoding
- Integrity markers (BITRAF64 header)
- Version tracking
- Compression ratio reporting

### 3. RAFCODE-Φ Encoder

**Purpose**: Code signing and provenance tracking system skeleton.

#### Installation

```bash
# No additional dependencies required (uses stdlib)
```

#### Usage

```bash
# Generate seal for a file
python tools/rafacode_encoder.py myfile.py --author @username

# Generate seal with provenance record
python tools/rafacode_encoder.py myfile.py --output provenance.json
```

#### Features

- SHA-256 content hashing
- Authorship tracking
- Timestamp generation
- Ethical compliance markers
- Provenance records
- Chain of custody tracking

### 4. Rafaelia Ethica[8] Engine

**Purpose**: Ethical validation engine that enforces human-protective norms.

#### Installation

```bash
# Install dependencies
pip install psutil
```

#### Usage

```bash
# Validate an operation
python tools/rafaelia_ethica_engine.py --validate operation.json --strict
```

#### Operation Descriptor Format

```json
{
  "type": "FILE_DELETE",
  "description": "Delete temporary files",
  "critical": false,
  "reversible": true,
  "documented": true,
  "logged": true,
  "has_loop": false,
  "has_bound": true,
  "fail_safe": true,
  "collects_data": false
}
```

#### Ethical Principles

1. **Human-Protective Norms**: Safety first
2. **No Infinite Loops**: Guaranteed termination
3. **Transparency**: Documented and logged
4. **Human-in-the-Loop**: Critical decisions require review
5. **Reversibility**: Create backups for irreversible operations
6. **Privacy by Default**: Minimal data collection
7. **Fail-Safe**: Graceful failure handling
8. **Documented Rationale**: Clear decision documentation

#### Verdicts

- `ALLOW`: Execute immediately
- `DENY`: Block execution
- `REQUIRE_REVIEW`: Human review needed
- `CONDITIONAL`: Execute with conditions

### 5. SBOM Generator

**Purpose**: Generate Software Bill of Materials for transparency.

#### Installation

```bash
# Install syft (recommended)
curl -sSfL https://raw.githubusercontent.com/anchore/syft/main/install.sh | sh -s -- -b /usr/local/bin

# Or use package manager
brew install syft  # macOS
```

#### Usage

```bash
# Generate SBOM for project
./tools/generate_sbom.sh

# Custom target and format
./tools/generate_sbom.sh ./app spdx-json app-sbom

# Available formats
./tools/generate_sbom.sh . cyclonedx-json
./tools/generate_sbom.sh . syft-json
```

#### Output Locations

SBOMs are generated in: `build/sbom/`

## 🚀 CI/CD Workflows

### Workflow: `validate-workflows.yml`

**Trigger**: Push or PR to `.github/workflows/`

**What it does**:
1. Checks out repository
2. Sets up Python environment
3. Installs dependencies (pyyaml)
4. Runs `check_workflows.py`
5. Displays validation summary
6. Uploads report artifact
7. Fails if critical errors found

**Artifacts**: `workflow-validation-report` (JSON)

### Workflow: `ci.yml`

**Trigger**: Push to main branches, PRs, manual dispatch

**What it does**:
1. Detects languages in repository
2. Runs matrix builds for detected languages:
   - **C**: gcc and clang
   - **Kotlin/Java**: Gradle build with multiple JDK versions
   - **Python**: Syntax checks with 3.9, 3.10, 3.11
   - **Shell**: shellcheck validation
3. Generates summary report

**Matrix Strategy**: Parallel execution with fail-fast disabled

## 🔐 Running Checks Locally

### Before Committing

```bash
# 1. Validate workflows
python tools/check_workflows.py

# 2. Check shell scripts
shellcheck scripts/*.sh tools/*.sh

# 3. Lint Kotlin code
./gradlew ktlint

# 4. Run tests
./gradlew testDebug

# 5. Check for secrets (if git-secrets installed)
git secrets --scan

# 6. Build APK (optional)
./gradlew assembleDebug
```

### One-Liner Check

```bash
python tools/check_workflows.py && \
find scripts tools -name "*.sh" -exec shellcheck {} \; && \
./gradlew ktlint && \
./gradlew testDebug && \
echo "✓ All checks passed"
```

## 🔒 Hash Verification

### Generate File Hashes

```bash
# SHA-256 hash of a file
sha256sum myfile.txt

# Multiple files
sha256sum tools/*.py > checksums.txt

# Verify checksums
sha256sum -c checksums.txt
```

### Generate Hashes for Release

```bash
# Create checksums for APK
sha256sum app/build/outputs/apk/debug/app-debug.apk > app-debug.sha256

# Create checksums for all artifacts
find build -type f -name "*.apk" -o -name "*.aab" | xargs sha256sum > checksums.txt
```

## 🔑 GPG Signing

### Setup GPG Key

```bash
# Generate GPG key
gpg --full-generate-key

# List keys
gpg --list-secret-keys --keyid-format=long

# Export public key
gpg --armor --export YOUR_KEY_ID > public-key.asc

# Configure Git to use GPG
git config --global user.signingkey YOUR_KEY_ID
git config --global commit.gpgsign true
```

### Sign Commits

```bash
# Sign a commit
git commit -S -m "Your commit message"

# Verify signature
git log --show-signature -1
```

### Sign Files

```bash
# Sign a file
gpg --detach-sign --armor myfile.apk

# Verify signature
gpg --verify myfile.apk.asc myfile.apk
```

## 🔐 Cosign Signing (Container/Artifact Signing)

### Install Cosign

```bash
# Download from GitHub releases
curl -LO https://github.com/sigstore/cosign/releases/latest/download/cosign-linux-amd64
chmod +x cosign-linux-amd64
sudo mv cosign-linux-amd64 /usr/local/bin/cosign

# Or use package manager
brew install cosign  # macOS
```

### Sign Artifacts

```bash
# Generate key pair
cosign generate-key-pair

# Sign file
cosign sign-blob --key cosign.key myfile.apk > myfile.sig

# Verify signature
cosign verify-blob --key cosign.pub --signature myfile.sig myfile.apk
```

## 📊 SBOM Analysis

### Generate and Analyze SBOM

```bash
# Generate SBOM
./tools/generate_sbom.sh

# View SBOM (if jq installed)
jq '.' build/sbom/sbom.json

# Count components
jq '.components | length' build/sbom/sbom.json

# List component names
jq -r '.components[].name' build/sbom/sbom.json

# Find specific component
jq '.components[] | select(.name == "kotlin-stdlib")' build/sbom/sbom.json
```

### SBOM Scanning for Vulnerabilities

```bash
# Install grype (vulnerability scanner)
curl -sSfL https://raw.githubusercontent.com/anchore/grype/main/install.sh | sh -s -- -b /usr/local/bin

# Scan SBOM for vulnerabilities
grype sbom:build/sbom/sbom.json

# Generate report
grype sbom:build/sbom/sbom.json -o json > vulnerabilities.json
```

## 🏗️ CI/CD Integration

### GitHub Actions Integration

```yaml
# Add to your workflow
- name: Validate workflows
  run: |
    pip install pyyaml
    python tools/check_workflows.py
    
- name: Run ethical validation
  run: |
    pip install psutil
    python tools/rafaelia_ethica_engine.py --validate operation.json
    
- name: Generate SBOM
  run: |
    ./tools/generate_sbom.sh
```

### Local CI Simulation

```bash
# Install act (run GitHub Actions locally)
# https://github.com/nektos/act

# Run workflow locally
act -j validate
```

## 📚 Additional Resources

### Documentation
- [Contributing Guidelines](../.github/CONTRIBUTING.md)
- [Security Policy](../.github/SECURITY.md)
- [Development Setup](./DEVELOPMENT_SETUP.md)

### Tools
- [syft](https://github.com/anchore/syft) - SBOM generation
- [grype](https://github.com/anchore/grype) - Vulnerability scanning
- [cosign](https://github.com/sigstore/cosign) - Artifact signing
- [git-secrets](https://github.com/awslabs/git-secrets) - Prevent secret commits
- [shellcheck](https://www.shellcheck.net/) - Shell script analysis

### Standards
- [CycloneDX](https://cyclonedx.org/) - SBOM standard
- [SPDX](https://spdx.dev/) - Software Package Data Exchange
- [SLSA](https://slsa.dev/) - Supply chain security framework
- [Sigstore](https://www.sigstore.dev/) - Software signing

## ❓ Troubleshooting

### Workflow Validator Issues

**Problem**: `ImportError: No module named 'yaml'`  
**Solution**: Install PyYAML: `pip install pyyaml`

**Problem**: Permission denied  
**Solution**: Make script executable: `chmod +x tools/check_workflows.py`

### SBOM Generator Issues

**Problem**: `syft: command not found`  
**Solution**: Install syft or script will generate fallback SBOM

**Problem**: Large project takes too long  
**Solution**: Target specific directories: `./tools/generate_sbom.sh ./app`

### Ethica Engine Issues

**Problem**: `ImportError: No module named 'psutil'`  
**Solution**: Install psutil: `pip install psutil`

## 🤝 Contributing

If you find issues with the validation tools or have suggestions for improvements:

1. Check existing issues
2. Create detailed bug report or feature request
3. Submit PR with fixes (follow contributing guidelines)

## 📞 Support

Questions? Contact:
- Create an issue: [GitHub Issues](https://github.com/rafaelmeloreisnovo/UserLAnd/issues)
- Security concerns: See [Security Policy](../.github/SECURITY.md)

---

**Documentation Version**: 1.0  
**Last Updated**: 2026-01-04  
**Maintainer**: @rafaelmeloreisnovo
