# Security Policy

**Author:** @rafaelmeloreisnovo  
**RAFCODE-Φ Seal:** ⟨Φ⟩-SECURITY-POLICY-v1.0

## 🔒 Reporting Security Vulnerabilities

We take security seriously. If you discover a security vulnerability, please report it responsibly.

### How to Report

**DO NOT** open a public GitHub issue for security vulnerabilities.

Instead, please report security issues to:

- **GitHub Security Advisory**: https://github.com/rafaelmeloreisnovo/UserLAnd/security/advisories/new

### What to Include in Your Report

1. **Description**: Clear description of the vulnerability
2. **Impact**: Potential impact and severity
3. **Steps to Reproduce**: Detailed steps to reproduce the issue
4. **Proof of Concept**: If available, provide PoC code or screenshots
5. **Suggested Fix**: If you have ideas for fixing it (optional)
6. **Your Contact**: How we can reach you for follow-up

### Example Report

```
Title: [Component] Brief description of vulnerability

Description:
The XYZ component allows unauthorized access to sensitive data through...

Impact:
An attacker could potentially [describe impact]. This affects versions X.Y.Z and later.

Steps to Reproduce:
1. Navigate to...
2. Execute the following...
3. Observe that...

Environment:
- OS: Android 12
- App Version: 2.9.1
- Device: Pixel 6

Suggested Fix:
Add input validation in [file.kt:line] to sanitize...
```

## ⏱️ Response Timeline

- **Initial Response**: Within 48 hours
- **Status Update**: Within 7 days
- **Fix Timeline**: Depends on severity
  - Critical: Within 7 days
  - High: Within 30 days
  - Medium: Within 90 days
  - Low: Next release cycle

## 🏆 Recognition

We appreciate security researchers who report vulnerabilities responsibly:

- ✨ Public recognition in release notes (if desired)
- 🎖️ Addition to security acknowledgments
- 📝 CVE assignment for significant vulnerabilities

## 🛡️ Security Best Practices for Contributors

### Code Review Guidelines

When contributing code, please ensure:

1. **Input Validation**: All user input is properly validated and sanitized
2. **No Hardcoded Secrets**: Never commit API keys, tokens, or passwords
3. **Dependency Management**: Use latest stable versions of dependencies
4. **Secure Communication**: Use HTTPS for all network communication
5. **Data Encryption**: Encrypt sensitive data at rest and in transit
6. **Least Privilege**: Request minimum necessary permissions

### Common Vulnerability Categories

Be aware of these common issues:

#### 🔴 Critical Issues
- Remote code execution (RCE)
- SQL injection
- Authentication bypass
- Arbitrary file access
- Privilege escalation

#### 🟠 High Priority Issues
- Cross-site scripting (XSS) in WebViews
- Insecure data storage
- Insecure communication channels
- Weak cryptography
- Command injection

#### 🟡 Medium Priority Issues
- Information disclosure
- Denial of service
- Insecure permissions
- Missing security headers
- Outdated dependencies with known CVEs

#### 🟢 Low Priority Issues
- Non-sensitive information leaks
- Weak configuration defaults
- Missing security best practices

## 🔍 Security Testing

### Before Submitting a PR

1. **Static Analysis**: Run static analysis tools
   ```bash
   ./gradlew ktlintCheck
   ```

2. **Dependency Check**: Review dependencies for known vulnerabilities
   ```bash
   ./gradlew dependencyCheckAnalyze
   ```

3. **CodeQL Scan**: Ensure CodeQL checks pass in CI

4. **Manual Review**: Review your code for security implications

### Security Scanning Tools

We use the following tools in our CI/CD pipeline:

- **CodeQL**: Semantic code analysis
- **Dependency scanning**: Known vulnerability detection
- **Workflow validation**: CI/CD pipeline security
- **Secret scanning**: Prevent credential leaks

## 📋 Security Checklist for PRs

- [ ] No hardcoded secrets or credentials
- [ ] All user input is validated
- [ ] Dependencies are up-to-date
- [ ] Cryptographic operations use secure algorithms
- [ ] Sensitive data is properly protected
- [ ] Error messages don't leak sensitive information
- [ ] Logging doesn't expose sensitive data
- [ ] Permissions follow least-privilege principle
- [ ] Security tests added for new features
- [ ] Documentation updated with security considerations

## 🔐 Secure Development Lifecycle

### Phase 1: Design
- Threat modeling
- Security requirements
- Privacy considerations

### Phase 2: Development
- Secure coding practices
- Code review
- Static analysis

### Phase 3: Testing
- Security testing
- Penetration testing
- Vulnerability scanning

### Phase 4: Deployment
- Secure configuration
- Monitoring setup
- Incident response plan

### Phase 5: Maintenance
- Security updates
- Patch management
- Continuous monitoring

## 🚨 Known Security Considerations

### Android Permissions

UserLAnd requires elevated permissions for its functionality:

- **WRITE_EXTERNAL_STORAGE**: For filesystem access in containers
- **INTERNET**: For package downloads and updates
- **FOREGROUND_SERVICE**: For background processes

These permissions are necessary for the app's core functionality.

### Root Access

UserLAnd provides Linux environments **without requiring device root**. The app:
- ✅ Runs entirely in userspace
- ✅ Uses proot for filesystem virtualization
- ✅ Does not modify system partitions
- ✅ Cannot access other apps' data

### Network Security

- All package downloads use HTTPS
- Repository signatures are verified
- TLS 1.2+ required for connections

## 📚 Additional Resources

- [OWASP Mobile Security Project](https://owasp.org/www-project-mobile-security/)
- [Android Security Best Practices](https://developer.android.com/topic/security/best-practices)
- [CWE Top 25](https://cwe.mitre.org/top25/)

## 📞 Contact

For security-related questions:
- **Security Advisories**: GitHub Security tab
- **General Security Questions**: Open a GitHub Discussion
- **Urgent Issues**: Use GitHub Security Advisory

## 🔄 Policy Updates

This security policy is reviewed and updated quarterly. Last updated: 2026-01-04

---

**Thank you for helping keep UserLAnd secure!** 🔒

See also:
- `.github/CONTRIBUTING.md` - Contribution guidelines
- `docs/README_checks.md` - CI/CD security checks
# Security Policy - Enhanced Edition
<!-- Author: @rafaelmeloreisnovo -->
<!-- RAFCODE-Φ: SECURITY-POLICY-v2.0 -->

## 🛡️ Security Philosophy

UserLAnd takes security seriously and follows these core principles:

1. **Defense in Depth**: Multiple layers of security controls
2. **Least Privilege**: Minimal permissions by default
3. **Secure by Default**: Security features enabled out of the box
4. **Transparency**: Open disclosure of security practices
5. **Rapid Response**: Fast patching of confirmed vulnerabilities

## 📊 Supported Versions

We actively support the following versions with security updates:

| Version | Supported          | Status |
| ------- | ------------------ | ------ |
| 2.8.x   | ✅ Yes             | Active |
| 2.7.x   | ✅ Yes             | Active |
| 2.6.x   | ⚠️  Critical only  | LTS    |
| < 2.6   | ❌ No              | EOL    |

## 🔒 Reporting a Vulnerability

### Quick Summary

**DO NOT** create a public GitHub issue for security vulnerabilities.

Instead, report privately through one of these channels:

1. **GitHub Security Advisories** (Preferred): [Security Tab](https://github.com/rafaelmeloreisnovo/UserLAnd/security/advisories/new)
2. **Email**: Contact maintainers directly (see CONTRIBUTING.md)
3. **Encrypted Email**: Use GPG keys available in the repository

### What to Include

Please provide:

- **Description**: Clear explanation of the vulnerability
- **Impact**: What an attacker could do (CVSS score if available)
- **Steps to Reproduce**: Detailed reproduction steps
- **Affected Versions**: Which versions are vulnerable
- **Environment**: OS, architecture, configuration details
- **Proof of Concept**: Code/script demonstrating the issue (if safe)
- **Suggested Fix**: Your ideas for remediation (optional)
- **Disclosure Timeline**: Your preferred disclosure timeline

### Example Report

```markdown
## Vulnerability: Remote Code Execution via Malformed Input

**Type**: RCE
**Severity**: Critical (CVSS 9.8)
**Affected Versions**: 2.7.0 - 2.8.1

### Description
The input parser in UserLAnd does not properly sanitize shell commands,
allowing an attacker to execute arbitrary code...

### Steps to Reproduce
1. Create a filesystem with name: `; rm -rf / #`
2. Start the filesystem
3. Observe command execution

### Impact
An attacker can execute arbitrary commands with app privileges...

### Suggested Fix
Implement proper input sanitization using parameterized commands...
```

## ⏱️ Response Timeline

After you report a vulnerability:

### 1. Acknowledgment (48 hours)
We will acknowledge receipt of your report and assign a tracking ID.

### 2. Assessment (5 business days)
We will:
- Confirm the vulnerability
- Assess severity and impact
- Determine affected versions
- Assign CVE if applicable

### 3. Fix Development (varies by severity)

| Severity | Target Fix Time | Target Patch Release |
| -------- | --------------- | -------------------- |
| Critical | 7 days          | Emergency release    |
| High     | 14 days         | Next minor release   |
| Medium   | 30 days         | Next minor release   |
| Low      | 90 days         | Best effort          |

### 4. Disclosure (after fix)
Once a fix is available:
- Security advisory published
- CVE details disclosed
- Credit given to reporter (if desired)
- Users notified of update

## 🏆 Security Researcher Recognition

We appreciate security researchers who:
- Report vulnerabilities responsibly
- Follow coordinated disclosure
- Help us protect our users

### Hall of Fame
We maintain a security hall of fame recognizing researchers who have helped improve UserLAnd security.

### Rewards
While we don't offer a formal bug bounty program, we:
- Publicly credit reporters (with permission)
- Provide detailed acknowledgment in release notes
- Consider donations/gifts for exceptional findings

## 🔐 Security Best Practices for Users

### Application Security
- ✅ Keep UserLAnd updated to the latest version
- ✅ Download only from official sources (Google Play, F-Droid, GitHub)
- ✅ Verify APK signatures before installation
- ✅ Review app permissions
- ✅ Use strong passwords for filesystems

### System Security
- ✅ Keep Android OS updated
- ✅ Use device encryption
- ✅ Enable screen lock
- ✅ Avoid rooted devices for sensitive work
- ✅ Use VPN on untrusted networks

### Data Security
- ✅ Backup important data regularly
- ✅ Encrypt sensitive files
- ✅ Don't store credentials in plaintext
- ✅ Use SSH keys instead of passwords
- ✅ Regularly rotate credentials

## 🛠️ Security Features

### Built-in Protections
- ✅ Isolated filesystems (proot containers)
- ✅ No root access required
- ✅ Sandboxed execution environment
- ✅ Secure SSH configuration
- ✅ Input sanitization
- ✅ Cryptographic verification of downloads

### CI/CD Security
- ✅ Automated security scanning (CodeQL)
- ✅ Dependency vulnerability checks
- ✅ Workflow validation
- ✅ Signed releases
- ✅ SBOM generation
- ✅ Provenance tracking

### Code Security
- ✅ Regular security audits
- ✅ Static analysis (Lint, ktlint)
- ✅ Dynamic testing
- ✅ Fuzz testing (where applicable)
- ✅ Penetration testing

## 🔍 Security Scanning

### Automated Scans

We run automated security scans on:
- Every commit (CodeQL)
- Pull requests (multiple scanners)
- Weekly scheduled scans
- Before releases

### Manual Audits

Security audits are performed:
- Before major releases
- After significant architectural changes
- Following security incident
- Annually (minimum)

## 📦 Dependency Security

### Dependency Management
- Use Dependabot for automated updates
- Review all dependency updates
- Pin dependency versions
- Scan dependencies for vulnerabilities
- Generate SBOM for transparency

### Vulnerability Response
When a dependency vulnerability is discovered:
1. Assess impact on UserLAnd
2. Update to patched version (if available)
3. Test compatibility
4. Release patch
5. Notify users if critical

## 🔐 Cryptography

### Algorithms Used
- **Hashing**: SHA-256, SHA-512
- **Encryption**: AES-256 (where applicable)
- **Signing**: RSA-4096, Ed25519
- **Key Derivation**: PBKDF2, Argon2

### Key Management
- Keys are never hardcoded
- Private keys stored securely
- Regular key rotation
- HSM for release signing (recommended)

## 📋 Security Checklist for Contributors

Before submitting code:

- [ ] No hardcoded secrets or credentials
- [ ] Input validation on all user input
- [ ] Output encoding to prevent injection
- [ ] Proper error handling (no info leakage)
- [ ] Secure defaults
- [ ] Least privilege principle
- [ ] No SQL injection vectors
- [ ] No command injection vectors
- [ ] No XSS vectors
- [ ] No path traversal vulnerabilities
- [ ] Proper authentication/authorization
- [ ] Secure random number generation
- [ ] No timing attack vectors
- [ ] Memory safety (in native code)
- [ ] Thread safety in concurrent code

## 🚨 Security Incidents

### If You Discover an Incident

If you discover that:
- A vulnerability is being actively exploited
- User data may have been compromised
- A security control has failed

**Immediately** contact maintainers through:
1. GitHub Security Advisory (mark as critical)
2. Emergency email contact
3. Direct message to @rafaelmeloreisnovo

### Incident Response

Our incident response process:
1. **Detect**: Identify and confirm incident
2. **Contain**: Limit damage and scope
3. **Eradicate**: Remove threat
4. **Recover**: Restore normal operations
5. **Learn**: Post-incident review and improvements

## 📚 Security Resources

### Documentation
- [Contributing Guidelines](.github/CONTRIBUTING.md)
- [Checks Documentation](docs/README_checks.md)
- [Technical Documentation](docs/TECHNICAL_DOCUMENTATION.md)

### Tools
- [Workflow Validator](tools/check_workflows.py)
- [Ethical Engine](tools/rafaelia_ethica_engine.py)
- [SBOM Generator](tools/generate_sbom.sh)

### External Resources
- [OWASP Mobile Security](https://owasp.org/www-project-mobile-security/)
- [Android Security Best Practices](https://developer.android.com/topic/security/best-practices)
- [CWE Database](https://cwe.mitre.org/)
- [CVE Database](https://cve.mitre.org/)

## 🤝 Security Partnerships

We collaborate with:
- Security research community
- Android security team
- Open source security initiatives
- CVE Numbering Authorities (CNAs)

## 📞 Contact

**Security Team**: @rafaelmeloreisnovo  
**GitHub Security**: [Security Advisories](https://github.com/rafaelmeloreisnovo/UserLAnd/security/advisories)

---

**Policy Version**: 2.0  
**Last Updated**: 2026-01-04  
**Next Review**: 2026-07-04

**Remember**: Security is everyone's responsibility. Thank you for helping keep UserLAnd secure! 🛡️
