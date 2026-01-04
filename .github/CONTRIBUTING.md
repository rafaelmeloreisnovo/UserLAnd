# Contributing to UserLAnd

**Author:** @rafaelmeloreisnovo  
**RAFCODE-Φ Seal:** ⟨Φ⟩-CONTRIBUTING-GUIDE-v1.0

Thank you for your interest in contributing to UserLAnd! This document outlines the guidelines and requirements for contributing to this project.

## 🔐 Code Signing & Commit Requirements

### Developer Certificate of Origin (DCO)

All commits must include a `Signed-off-by` line to certify that you have the right to submit the contribution under the project's license. Add this to your commits:

```bash
git commit -s -m "Your commit message"
```

This adds a line like: `Signed-off-by: Your Name <your.email@example.com>`

### GPG Commit Signing (Recommended)

For enhanced security, we strongly recommend GPG signing your commits:

```bash
# Configure Git to sign commits
git config --global user.signingkey YOUR_GPG_KEY_ID
git config --global commit.gpgsign true

# Create a signed commit
git commit -S -m "Your commit message"
```

**Verify your signature:**
```bash
git log --show-signature -1
```

## 🛡️ Protected Files & Nuclear Assets

The following files are considered "nuclear" assets and require special review:

- `.github/workflows/**` - CI/CD pipeline definitions
- `gradle/wrapper/**` - Build system integrity
- `signing.properties*` - Code signing configuration
- `CODEOWNERS` - Repository governance
- `.github/SECURITY.md` - Security policies
- Core security-critical source files

**Requirements for changes to protected files:**
1. ✅ Must pass all CI checks
2. ✅ Must be reviewed by @rafaelmeloreisnovo
3. ✅ Must include detailed explanation of changes
4. ✅ Must maintain existing security controls
5. ✅ Cannot bypass existing checks

## 📋 Pull Request Process

### Before Creating a PR

1. **Fork and Branch**: Create a feature branch from `main`
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Run Local Tests**: Ensure all tests pass
   ```bash
   ./gradlew test
   ./gradlew assembleDebug
   ```

3. **Lint Your Code**: Follow code style guidelines
   ```bash
   ./gradlew ktlintCheck
   ```

4. **Sign Your Commits**: Use DCO (`-s`) and GPG (`-S`) flags

### PR Template Checklist

When you create a PR, please complete the checklist in the template:

- [ ] Code follows project style guidelines
- [ ] All tests pass locally
- [ ] Added/updated tests for changes
- [ ] Documentation updated if needed
- [ ] Commits are signed off (DCO)
- [ ] No hardcoded secrets or credentials
- [ ] Security implications reviewed
- [ ] CI checks pass

### Review Process

1. **Automated Checks**: CI must pass (builds, tests, linting, security scans)
2. **Code Review**: At least one approval required
3. **Maintainer Review**: Changes to protected files need @rafaelmeloreisnovo approval
4. **Merge**: Squash and merge (preserves clean history)

## 🔍 Mandatory Checks

All PRs must pass:

- ✅ **validate-workflows** - Workflow file integrity
- ✅ **ci** - Multi-language build matrix
- ✅ **CodeQL** - Security scanning
- ✅ **Android builds** - APK compilation
- ✅ **Unit tests** - Test suite execution

## 🔒 Security Guidelines

### What NOT to Commit

- ❌ API keys, tokens, or passwords
- ❌ Private keys or keystores
- ❌ Personal credentials
- ❌ Sensitive configuration files
- ❌ `.jks` or `.keystore` files

### Security Best Practices

1. **Input Validation**: Always validate user input
2. **Dependency Updates**: Keep dependencies current
3. **Secrets Management**: Use GitHub Secrets for CI/CD
4. **Code Review**: Security-sensitive changes need extra scrutiny
5. **Vulnerability Reports**: See `.github/SECURITY.md`

## 🏗️ Development Setup

### Prerequisites

- JDK 17 or later
- Android SDK (API 30+)
- Gradle 8.7+
- Git with GPG setup (optional but recommended)

### Building Locally

```bash
# Clone the repository
git clone https://github.com/rafaelmeloreisnovo/UserLAnd.git
cd UserLAnd

# Build debug APK
./gradlew assembleDebug

# Run tests
./gradlew test

# Run lint checks
./gradlew ktlintCheck
```

### Running Native Code

```bash
# Compile C files (if working on native components)
cd /path/to/c/files
gcc -o output_file source_file.c
```

## 📝 Commit Message Guidelines

Follow conventional commits:

```
type(scope): subject

body (optional)

footer (optional)
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting)
- `refactor`: Code refactoring
- `test`: Test additions/changes
- `chore`: Build process or tooling changes

**Example:**
```
feat(ui): add dark mode support

Implements user-requested dark theme for main activity.
Adds toggle in settings and persists preference.

Signed-off-by: Your Name <your.email@example.com>
```

## 🎯 Code Style

### Kotlin
- Follow [Kotlin coding conventions](https://kotlinlang.org/docs/coding-conventions.html)
- Use ktlint for formatting
- Maximum line length: 120 characters

### Java
- Follow [Google Java Style Guide](https://google.github.io/styleguide/javaguide.html)
- Use 4 spaces for indentation

### Python
- Follow PEP 8
- Use type hints where applicable
- Maximum line length: 120 characters

### Shell Scripts
- Use `#!/bin/bash` shebang
- Use `set -euo pipefail` for safety
- Pass shellcheck validation

## 🤝 Code of Conduct

Please note that this project follows a [Code of Conduct](../CODE_OF_CONDUCT.md). By participating, you agree to uphold this code.

## ❓ Questions?

- **Issues**: Use GitHub Issues for bugs and feature requests
- **Discussions**: Use GitHub Discussions for questions
- **Security**: See `.github/SECURITY.md` for vulnerability reporting

## 📜 License

By contributing, you agree that your contributions will be licensed under the same license as the project (see LICENSE file).

---

**Thank you for contributing to UserLAnd!** 🎉

For additional technical documentation, see:
- `docs/DEVELOPMENT_SETUP.md`
- `docs/TECHNICAL_DOCUMENTATION.md`
- `docs/README_checks.md` (for CI/CD and integrity checks)
