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
# Contributing to UserLAnd - Governance Edition
<!-- Author: @rafaelmeloreisnovo -->
<!-- RAFCODE-Φ: CONTRIBUTING-GOVERNANCE-v1.0 -->

## ⚖️ Governance and Ethical Compliance

This document extends the main CONTRIBUTING.md with governance rules, ethical compliance requirements, and file protection policies.

### Core Principles

1. **Human-Protective Norm Prevails**: All contributions must adhere to norms that protect human welfare, privacy, and rights.
2. **Transparent Decision Making**: All decisions must be documented with clear rationale.
3. **No Harmful Loops**: Code must not contain infinite loops or resource exhaustion patterns.
4. **Requires Human Review**: Critical changes require explicit human approval.
5. **Reversible Actions**: Changes should be reversible where possible.
6. **Privacy by Default**: Data collection must be minimal and consensual.
7. **Fail-Safe Mechanisms**: Code must handle failures gracefully.
8. **Documented Rationale**: All architectural decisions must be documented.

## 🔐 Sign-off and Authorship

### Developer Certificate of Origin (DCO)

All commits must be signed off to certify that you have the right to submit the code under the project's license.

Add a `Signed-off-by` line to your commit messages:

```
git commit -s -m "Your commit message"
```

This adds:
```
Signed-off-by: Your Name <your.email@example.com>
```

### GPG Signing (Recommended)

For enhanced security and authorship verification, we recommend GPG-signing your commits:

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
# Commit with signature
git commit -S -m "Your commit message"
```

## 🛡️ Protected Files and Nuclear Assets

### Critical Files Requiring Special Approval

The following files and directories are considered "nuclear assets" and require:
- **Mandatory code review** by at least 2 maintainers
- **Explicit approval** from @rafaelmeloreisnovo
- **Rationale documentation** for any changes

Protected paths:
- `.github/workflows/*.yml` - CI/CD workflows
- `tools/check_workflows.py` - Workflow validator
- `tools/rafaelia_ethica_engine.py` - Ethical engine
- `tools/*_encoder.py` - Encoding/signing systems
- `CODEOWNERS` - Ownership definitions
- `.github/SECURITY.md` - Security policies
- Build and signing configurations

### File Protection Enforcement

Changes to protected files will trigger:
1. Automated validation via `validate-workflows.yml`
2. Mandatory human review (no auto-merge)
3. Additional security scans
4. Approval gate requiring maintainer consent

## 📋 Pull Request Requirements

### Compliance Checklist

Every PR must include:

- [ ] **Signed-off-by** line in all commits (DCO compliance)
- [ ] **GPG signature** (recommended but not mandatory)
- [ ] **No secrets** committed in code or history
- [ ] **Tests** for new functionality (where applicable)
- [ ] **Documentation** updated for user-facing changes
- [ ] **Ethical validation**: No harmful patterns, no infinite loops
- [ ] **Rationale** documented for architectural changes
- [ ] **Breaking changes** clearly marked and justified
- [ ] **License compatibility** verified for new dependencies

### Workflow Status Checks

All PRs must pass:
- ✅ Workflow validation (`validate-workflows.yml`)
- ✅ CI matrix builds (`ci.yml`)
- ✅ Security scans (CodeQL)
- ✅ Lint checks
- ✅ Unit tests

### Review Requirements

- **Standard PRs**: 1 approving review from a maintainer
- **Protected files**: 2 approving reviews + explicit @rafaelmeloreisnovo approval
- **Security fixes**: Fast-track with immediate maintainer review
- **Documentation only**: 1 approving review

## 🔒 Security and Vulnerability Reporting

See `.github/SECURITY.md` for detailed instructions on reporting security vulnerabilities.

**Never commit**:
- Passwords, API keys, tokens
- Private keys or certificates
- Signing keys or keystores
- Personal identifiable information (PII)
- Credentials of any kind

## 🏗️ Contribution Workflow

### 1. Create Issue
- Describe the problem or enhancement
- Link to related issues/discussions
- Assign yourself to the ticket

### 2. Create Branch
```bash
# Create branch from master
git checkout master
git pull origin master
git checkout -b feature/your-feature-name

# Or for fixes
git checkout -b fix/issue-number-description
```

### 3. Make Changes
- Follow existing code style and conventions
- Write clear, atomic commits
- Sign off all commits
- Run local tests and lints

### 4. Submit Pull Request
- Use the PR template
- Fill out the compliance checklist
- Request review from appropriate maintainers
- Link to related issues

### 5. Address Review Feedback
- Respond to all review comments
- Make requested changes
- Re-request review after updates
- Do not force-push after review starts

### 6. Merge
- Maintainers will merge once approved
- Use "Squash and merge" for master
- Branch will be deleted automatically

## 🔧 Local Development Best Practices

### Before Committing

```bash
# Run linters
./gradlew ktlint

# Run tests
./gradlew testDebug

# Check shell scripts
shellcheck scripts/*.sh

# Validate workflows locally
python tools/check_workflows.py

# Check for secrets (install git-secrets first)
git secrets --scan
```

### Commit Message Format

```
type(scope): subject line (max 72 chars)

Detailed description of what changed and why.
Reference related issues using #issue-number.

Signed-off-by: Your Name <your.email@example.com>
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `chore`

## 📊 Metrics and Quality Gates

### Code Coverage
- Maintain or improve existing coverage
- New features should include tests

### Performance
- No performance regressions
- Benchmark critical paths

### Security
- Pass all security scans
- Address CodeQL findings

## 🌐 Community Guidelines

### Code of Conduct
All interactions must follow the [Code of Conduct](../CODE_OF_CONDUCT.md).

### Communication
- Be respectful and constructive
- Assume good intentions
- Provide clear, actionable feedback
- Help others learn and grow

## 📚 Resources

- [Main Contributing Guide](../CONTRIBUTING.md)
- [Security Policy](.github/SECURITY.md)
- [Development Setup](../docs/DEVELOPMENT_SETUP.md)
- [Technical Documentation](../docs/TECHNICAL_DOCUMENTATION.md)

## ⚖️ License and Legal

By contributing, you agree that your contributions will be licensed under the project's license (see LICENSE file).

You must have the legal right to submit your contributions and agree that you are not violating any third-party rights, patents, or copyrights.

---

**Questions?** Open an issue or reach out to the maintainers.

**Governance Version**: 1.0  
**Last Updated**: 2026-01-04  
**Maintainer**: @rafaelmeloreisnovo
