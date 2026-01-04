# Contributing to UserLAnd

**Author:** @rafaelmeloreisnovo  
**RAFCODE-Φ seal:** PROTOTYPE-DRAFT-v0.1.0  
**Status:** Draft for review - requires maintainer approval

---

Thank you for your interest in contributing to UserLAnd! This guide helps you understand how to contribute effectively to the project.

## 🔐 Important Notice

**This document is a PROTOTYPE/DRAFT.** All workflows, checks, and policies defined here require explicit approval from the repository maintainer before being enforced.

**RAFCODE-Φ seal guarantees:**
- Authorship: @rafaelmeloreisnovo
- No automatic execution of infinite loops
- Ethica[8] enforcement active

## Code of Conduct

All contributions must follow the UserLAnd [Code of Conduct](https://github.com/CypherpunkArmory/UserLAnd/blob/master/CODE_OF_CONDUCT.md).

## How to Contribute

### 1. Before You Start

- Search existing [issues](https://github.com/rafaelmeloreisnovo/UserLAnd/issues) to avoid duplicates
- For major changes, open an issue first to discuss your approach
- Ensure your changes align with the project's goals

### 2. Setting Up Your Environment

```bash
# Fork and clone the repository
git clone https://github.com/YOUR_USERNAME/UserLAnd.git
cd UserLAnd

# Set up development environment
# See docs/DEVELOPMENT_SETUP.md for detailed instructions

# Install dependencies
./gradlew build
```

### 3. Making Changes

**Branch Strategy:**
- Create a feature branch from `master`
- Use descriptive branch names: `feature/add-xyz`, `fix/issue-123`
- Keep branches focused on a single concern

**Coding Standards:**
- Follow MVVM-C architecture pattern
- Keep UI updates in XML and view controllers
- Decouple business logic from Android framework
- Place logic in appropriate packages (`model`, `utils`, `viewmodel`)

**Commit Messages:**
- Use present tense: "Add feature" not "Added feature"
- Be descriptive but concise
- Reference issues: "Fix #123: Resolve crash on startup"

### 4. Testing Your Changes

```bash
# Run unit tests
./gradlew test

# Run all tests
./gradlew testAll

# Check code style
./gradlew ktlint
```

### 5. Submitting a Pull Request

**Required Status Checks (Draft - Pending Approval):**
- ✅ `validate-workflows` - Workflow validation
- ✅ `ci` - CI matrix tests
- ✅ Code review approval
- ✅ Signed commits (DCO)
- ✅ All tests passing

**PR Requirements:**
- Use the PR template (`.github/pull_request_template.md`)
- Fill out all sections completely
- Link related issues
- Describe testing performed
- Explain the value to users

**Branch Protection (Proposed):**
- Require pull request reviews before merging
- Require status checks to pass
- Require signed commits
- No force pushes to protected branches

### 6. Review Process

1. Maintainers will review your PR
2. Address any feedback or requested changes
3. Keep commits clean (squash if needed)
4. Once approved, a maintainer will merge your PR

## Development Guidelines

### Architecture

UserLAnd follows **MVVM-C** (Model-View-ViewModel-Coordinator):

- **Model**: Data and business logic (decoupled from Android)
- **View**: XML layouts and visual representation
- **ViewModel**: Application state and logic
- **Coordinator**: Navigation and flow control

### Code Style

**Kotlin:**
- Use `ktlint` for formatting
- Run `./gradlew ktlint` before committing
- Follow [Kotlin coding conventions](https://kotlinlang.org/docs/coding-conventions.html)

**Structure:**
```
app/src/main/java/tech/ula/
├── model/          # Business logic, data classes
├── ui/             # Activities, Fragments, ViewModels
├── utils/          # Helper functions, utilities
└── viewmodel/      # ViewModels for MVVM pattern
```

### Testing

- Write unit tests for business logic
- Write integration tests for complex interactions
- Aim for meaningful coverage, not 100% coverage
- Use mocks appropriately

### Documentation

- Update README.md for user-facing changes
- Update technical docs in `docs/` for architectural changes
- Add inline comments for complex logic
- Keep comments concise and meaningful

## Workflow and CI (PROTOTYPE)

**⚠️ IMPORTANT:** These workflows are prototypes/drafts. They will not automatically enforce policies until approved by the maintainer.

### Automated Checks

1. **Workflow Validation** (`validate-workflows`)
   - Validates GitHub Actions YAML syntax
   - Checks for security best practices
   - Runs Ethica[8] validation

2. **CI Matrix** (`ci`)
   - Builds on multiple OS configurations
   - Runs tests across Java versions
   - Generates SBOM (Software Bill of Materials)

3. **Security Checks**
   - Workflow security validation
   - Dependency scanning
   - No infinite loops (Ethica[8])

### Status Checks

Before merging, ensure:
- [ ] All CI checks pass
- [ ] Code review approved
- [ ] Commits are signed
- [ ] Documentation updated
- [ ] Tests added/updated

## Getting Help

- **Issues**: [GitHub Issues](https://github.com/rafaelmeloreisnovo/UserLAnd/issues)
- **Discussions**: [GitHub Discussions](https://github.com/rafaelmeloreisnovo/UserLAnd/discussions)
- **Documentation**: Check `docs/` directory
- **Wiki**: [Project Wiki](https://github.com/CypherpunkArmory/UserLAnd/wiki)

## RAFAELIA Tools (Prototype)

This repository includes prototype RAFAELIA tools:

- `tools/check_workflows.py` - Workflow validator
- `tools/rafaelia_ethica_engine.py` - Ethica[8] enforcement
- `tools/bitraf64_compressor.py` - BITRAF64 compression
- `tools/bitraf64_decompressor.py` - BITRAF64 decompression
- `tools/rafacode_encoder.py` - RAFCODE-Φ encoder
- `tools/generate_sbom.sh` - SBOM generator

All tools are marked as PROTOTYPE-DRAFT and require testing before production use.

## Security

See [SECURITY.md](.github/SECURITY.md) for security policies and reporting vulnerabilities.

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.

---

**RAFCODE-Φ seal:** @rafaelmeloreisnovo  
**Status:** PROTOTYPE-DRAFT  
**DO NOT MERGE** without explicit maintainer approval  
**All artifacts preserve authorship and require human review before activation**
