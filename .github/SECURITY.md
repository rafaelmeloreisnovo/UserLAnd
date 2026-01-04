# Security Policy

**Author:** @rafaelmeloreisnovo  
**RAFCODE-Φ seal:** PROTOTYPE-DRAFT-v0.1.0  
**Status:** Draft for review - requires maintainer approval

---

## 🔐 Important Notice

**This security policy is a PROTOTYPE/DRAFT.** All security measures and workflows defined here require explicit approval from the repository maintainer before being enforced.

**RAFCODE-Φ seal guarantees:**
- Authorship: @rafaelmeloreisnovo
- No automatic execution of infinite loops
- Ethica[8] enforcement active
- Security-first approach

---

## Supported Versions

We actively support the following versions of UserLAnd with security updates:

| Version | Supported          | Status |
| ------- | ------------------ | ------ |
| 2.8.x   | ✅ Yes             | Active |
| 2.7.x   | ✅ Yes             | Active |
| < 2.7   | ❌ No              | EOL    |

## Reporting a Vulnerability

We take security seriously. If you discover a vulnerability, please follow these steps:

### 1. Do Not Disclose Publicly

**DO NOT** create a public GitHub issue for security vulnerabilities. This protects users until a fix is available.

### 2. Report Privately

Report security vulnerabilities through:

- **GitHub Security Advisories**: Use the [Security tab](https://github.com/rafaelmeloreisnovo/UserLAnd/security/advisories) (preferred)
- **Email**: Contact maintainers directly (see repository contacts)

### 3. Include Relevant Information

When reporting, include:

- **Description**: Clear description of the vulnerability
- **Impact**: What an attacker could do by exploiting this
- **Steps to Reproduce**: Detailed reproduction steps
- **Affected Versions**: Which versions are affected
- **Suggested Fix**: Your ideas for fixing it (optional)
- **Proof of Concept**: If applicable and safe to share
- **CVSS Score**: If you've calculated one (optional)

### 4. What to Expect

After reporting:

1. **Acknowledgment**: Receipt confirmation within 48 hours
2. **Assessment**: Severity evaluation within 5 business days
3. **Fix Timeline**:
   - Critical: Within 7 days
   - High: Within 14 days
   - Medium: Within 30 days
   - Low: Best effort basis
4. **Disclosure**: After fix:
   - Release patched version
   - Publish security advisory
   - Credit reporter (if desired)

## Security Best Practices for Users

### Keeping UserLAnd Secure

1. **Stay Updated**: Always use the latest version
2. **Review Permissions**: Understand requested permissions
3. **Strong Credentials**: Use strong passwords for SSH/filesystems
4. **Regular Backups**: Backup important filesystem data
5. **Trusted Sources**: Only install from trusted repositories

### Security Considerations

#### Root Access
- UserLAnd **does not require** root access
- Apps run in contained environment
- Isolation provides security with some functionality limits

#### Network Security
- SSH/VNC sessions transmit over network
- Use encrypted connections for remote access
- Be cautious with untrusted networks

#### File Permissions
- Files stored in Android app data directory
- Standard Android sandboxing applies
- Uninstalling UserLAnd removes all data

## Security Features (Prototype)

### Current Security Measures

1. **No Root Required**: Runs entirely in userspace
2. **App Sandboxing**: Leverages Android's isolation
3. **Secure Build Pipeline**:
   - Automated CI/CD with GitHub Actions
   - Reproducible builds
   - Code review requirements
4. **Dependency Management**: Regular updates
5. **Code Signing**: All official releases signed

### Planned Security Enhancements (PROTOTYPE)

The following are **prototype implementations** pending approval:

- [x] **Workflow Validation**: `validate-workflows.yml` (draft)
  - Validates GitHub Actions for security issues
  - Checks for script injection vulnerabilities
  - Verifies action version pinning

- [x] **Ethica[8] Engine**: `rafaelia_ethica_engine.py` (draft)
  - Prevents infinite loops
  - Blocks unsafe operations
  - Enforces resource limits

- [x] **CI Security Matrix**: `ci.yml` (draft)
  - Multi-platform testing
  - Security checks
  - SBOM generation

- [ ] **CodeQL Analysis**: Integration pending
- [ ] **Secret Scanning**: Configuration pending
- [ ] **Dependency Scanning**: Automation pending
- [ ] **SBOM Automation**: Full implementation pending

### RAFAELIA Security Tools (Prototype)

**⚠️ All tools are PROTOTYPE/DRAFT status:**

1. **Workflow Validator** (`tools/check_workflows.py`)
   - Validates workflow security
   - Checks for common vulnerabilities
   - Ensures best practices

2. **Ethica[8] Engine** (`tools/rafaelia_ethica_engine.py`)
   - AST-based code analysis
   - Infinite loop detection
   - Unsafe operation blocking

3. **BITRAF64 Tools**
   - `bitraf64_compressor.py`: Secure compression
   - `bitraf64_decompressor.py`: Secure decompression
   - Metadata embedding with RAFCODE-Φ seals

4. **RAFCODE Encoder** (`tools/rafacode_encoder.py`)
   - Cryptographic seals
   - Provenance tracking
   - Authorship verification

5. **SBOM Generator** (`tools/generate_sbom.sh`)
   - Software Bill of Materials
   - Dependency tracking
   - Supply chain security

## Security-Related Configuration

### Build Security

When building from source:

1. **Never commit secrets**: Use `signing.properties.template`
2. **Verify checksums**: Check downloaded SDKs/tools
3. **Use official sources**: Download from official sites only
4. **Review dependencies**: Check `app/build.gradle`

### Signing Security

- Keep signing keys secure and backed up
- Never share `signing.properties` or keystores
- Use strong keystore passwords
- Consider HSM for production signing

### Branch Protection (Proposed - DRAFT)

**⚠️ Pending maintainer approval:**

Recommended settings:
- Require pull request reviews (1+ approvers)
- Require status checks:
  - `validate-workflows` (workflow security)
  - `ci` (CI matrix tests)
- Require signed commits (DCO)
- No force pushes
- No deletions
- Require linear history (optional)

### Commit Signing (Recommended)

Sign commits with GPG or SSH keys:

```bash
# Configure Git signing
git config --global commit.gpgsign true
git config --global user.signingkey YOUR_KEY_ID

# Verify signing
git log --show-signature
```

## Third-Party Dependencies

UserLAnd relies on third-party libraries. We:

- Regularly review and update dependencies
- Monitor security advisories
- Test updates before deployment
- Document known vulnerabilities

**Key dependencies:**
- Android Gradle Plugin
- Kotlin Standard Library
- AndroidX libraries
- Navigation Component
- Room Database
- OkHttp
- Moshi

## Scope

This security policy covers:

✅ **In Scope:**
- UserLAnd Android application
- Build scripts and CI/CD pipelines
- RAFAELIA telemetry components
- Static helper binaries
- Documentation and examples
- GitHub Actions workflows

❌ **Out of Scope:**
- Third-party apps installed in UserLAnd
- User-created scripts/configurations
- Specific Linux distribution issues
- Android OS vulnerabilities

## Vulnerability Disclosure Timeline

Our commitment:

1. **Private disclosure period**: 90 days from report
2. **Coordinated disclosure**: After fix deployment
3. **Public advisory**: Published after patch release
4. **CVE assignment**: For qualifying vulnerabilities

## Ethica[8] Security Principles

**RAFCODE-Φ sealed security principles:**

1. **No infinite loops**: Prevented by Ethica[8]
2. **Resource limits**: Enforced at runtime
3. **Safe defaults**: Security-first configuration
4. **Fail securely**: Graceful degradation
5. **Least privilege**: Minimal permissions
6. **Defense in depth**: Multiple security layers
7. **Transparency**: Open security practices
8. **Human approval**: No automatic critical actions

## Questions?

For security questions:

1. Check [FAQ in Wiki](https://github.com/CypherpunkArmory/UserLAnd/wiki)
2. Ask in [Discussions](https://github.com/rafaelmeloreisnovo/UserLAnd/discussions)
3. Review [Issues](https://github.com/rafaelmeloreisnovo/UserLAnd/issues)
4. Contact maintainers privately for sensitive matters

## Acknowledgments

We appreciate responsible disclosure from security researchers. Contributors will be credited in:

- Security advisories
- CHANGELOG.md
- Hall of Fame (if applicable)

---

**RAFCODE-Φ seal:** @rafaelmeloreisnovo  
**Status:** PROTOTYPE-DRAFT  
**DO NOT MERGE** without explicit maintainer approval  
**All security measures require human review before activation**

Thank you for helping keep UserLAnd and its users safe!
