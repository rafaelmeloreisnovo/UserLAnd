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
