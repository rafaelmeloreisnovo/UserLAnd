# Security Policy

## Supported Versions

We actively support the following versions of UserLAnd with security updates:

| Version | Supported          |
| ------- | ------------------ |
| 2.8.x   | :white_check_mark: |
| 2.7.x   | :white_check_mark: |
| < 2.7   | :x:                |

## Reporting a Vulnerability

We take the security of UserLAnd seriously. If you discover a security vulnerability, please follow these steps:

### 1. Do Not Disclose Publicly

Please **do not** create a public GitHub issue for security vulnerabilities. This helps protect users until a fix is available.

### 2. Report Privately

Report security vulnerabilities through one of these channels:

- **GitHub Security Advisories**: Use the [Security tab](https://github.com/CypherpunkArmory/UserLAnd/security/advisories) to privately report vulnerabilities
- **Email**: Contact the maintainers directly (check CONTRIBUTING.md for contact information)

### 3. Include Relevant Information

When reporting a vulnerability, please include:

- **Description**: Clear description of the vulnerability
- **Impact**: What an attacker could do by exploiting this
- **Steps to Reproduce**: Detailed steps to reproduce the issue
- **Affected Versions**: Which versions are affected
- **Suggested Fix**: If you have ideas on how to fix it (optional)
- **Proof of Concept**: If applicable and safe to share

### 4. What to Expect

After you report a vulnerability:

1. **Acknowledgment**: We'll acknowledge receipt within 48 hours
2. **Assessment**: We'll assess the severity and impact within 5 business days
3. **Fix Timeline**: We'll work on a fix with target timelines:
   - Critical: Within 7 days
   - High: Within 14 days
   - Medium: Within 30 days
   - Low: Best effort basis
4. **Disclosure**: Once fixed, we'll:
   - Release a patched version
   - Publish a security advisory
   - Credit you for the discovery (if desired)

## Security Best Practices for Users

### Keeping UserLAnd Secure

1. **Keep Updated**: Always use the latest version from Google Play Store or GitHub releases
2. **App Permissions**: Review and understand the permissions UserLAnd requests
3. **Secure Credentials**: Use strong passwords for SSH sessions and filesystems
4. **Data Backup**: Regularly backup important data from your filesystems
5. **Trusted Sources**: Only install applications from trusted repositories within UserLAnd

### Known Security Considerations

#### Root Access
- UserLAnd does **not require** root access on your Android device
- Applications run in a contained environment without elevated privileges
- This isolation provides security but may limit some functionality

#### Network Security
- SSH and VNC sessions transmit data over the network
- Consider using encrypted connections when accessing sessions remotely
- Be cautious when exposing services to untrusted networks

#### File Permissions
- Files are stored in your Android app data directory
- Standard Android app sandboxing applies
- Uninstalling UserLAnd will remove all filesystems and data

## Security Features

### Current Security Measures

1. **No Root Required**: Runs entirely in userspace without requiring device root
2. **App Sandboxing**: Leverages Android's app sandboxing for isolation
3. **Secure Build Pipeline**: 
   - Automated CI/CD with GitHub Actions
   - Reproducible builds with pinned dependency versions
   - Code review requirements for pull requests
4. **Dependency Management**: Regular updates of dependencies
5. **Code Signing**: All official releases are properly signed

### Planned Security Enhancements

- [ ] Automated dependency vulnerability scanning
- [ ] CodeQL security analysis in CI pipeline
- [ ] Secret scanning to prevent credential leaks
- [ ] Regular security audits of critical components
- [ ] Enhanced encryption options for stored data

## Security-Related Configuration

### Build Security

When building from source:

1. **Never commit secrets**: Use `signing.properties.template` as a guide
2. **Verify checksums**: Verify downloaded SDKs and tools
3. **Use official sources**: Only download build tools from official sources
4. **Review dependencies**: Check `app/build.gradle` for dependency versions

### Signing Security

- Keep your signing keys secure and backed up
- Never share your `signing.properties` or keystore files
- Use strong passwords for keystores
- Consider using a hardware security module (HSM) for production signing

## Third-Party Dependencies

UserLAnd relies on various third-party libraries. We:

- Regularly review and update dependencies
- Monitor security advisories for used libraries
- Test updates before deployment
- Document known vulnerabilities and workarounds

Key dependencies to monitor:

- Android Gradle Plugin
- Kotlin Standard Library
- AndroidX libraries
- Navigation Component
- Room Database
- OkHttp
- Moshi

## Scope

This security policy covers:

- The UserLAnd Android application
- Build scripts and CI/CD pipelines
- RAFAELIA telemetry components
- Static helper binaries
- Documentation and examples

This policy does **not** cover:

- Third-party applications installed within UserLAnd
- User-created scripts or configurations
- Issues with specific Linux distributions
- Android OS vulnerabilities

## Questions?

If you have questions about this security policy or UserLAnd's security in general:

1. Check the [FAQ in the Wiki](https://github.com/CypherpunkArmory/UserLAnd/wiki)
2. Ask in [GitHub Discussions](https://github.com/CypherpunkArmory/UserLAnd/discussions)
3. Review existing [Issues](https://github.com/CypherpunkArmory/UserLAnd/issues)

Thank you for helping keep UserLAnd and its users safe!
