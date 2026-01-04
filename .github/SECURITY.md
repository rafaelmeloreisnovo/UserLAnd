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
