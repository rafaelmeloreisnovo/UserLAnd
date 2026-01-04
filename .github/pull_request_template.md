# Pull Request
<!-- Author: @rafaelmeloreisnovo -->
<!-- RAFCODE-Φ: PR-TEMPLATE-v1.0 -->

## 📋 Description

<!-- Provide a clear and concise description of what this PR does -->

## 🎯 Related Issues

<!-- Link to related issues using #issue-number -->
Closes #
Relates to #

## 🔄 Type of Change

<!-- Mark the relevant option(s) with an 'x' -->

- [ ] 🐛 Bug fix (non-breaking change that fixes an issue)
- [ ] ✨ New feature (non-breaking change that adds functionality)
- [ ] 💥 Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] 📝 Documentation update
- [ ] 🎨 Code style/refactoring (no functional changes)
- [ ] ⚡ Performance improvement
- [ ] 🧪 Test update
- [ ] 🔧 Build/CI configuration change
- [ ] 🔒 Security fix

## ✅ Compliance Checklist

<!-- ALL items must be checked before requesting review -->

### Code Quality
- [ ] My code follows the project's style guidelines
- [ ] I have performed a self-review of my own code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] My changes generate no new warnings or errors
- [ ] I have run linters (`./gradlew ktlint`, `shellcheck`, etc.)

### Testing
- [ ] I have added tests that prove my fix is effective or that my feature works
- [ ] New and existing unit tests pass locally with my changes
- [ ] I have manually tested the changes
- [ ] I have tested on relevant Android versions (if applicable)

### Documentation
- [ ] I have updated relevant documentation
- [ ] I have updated CHANGELOG.md (if applicable)
- [ ] I have updated README.md (if needed)

### Security and Ethics
- [ ] **No secrets committed** (passwords, tokens, keys, credentials)
- [ ] **No infinite loops** or resource exhaustion patterns
- [ ] **Input validation** implemented for user input
- [ ] **No security vulnerabilities** introduced (checked with CodeQL/scanners)
- [ ] **Ethical compliance**: Changes adhere to human-protective norms

### Authorship and Legal
- [ ] **Signed-off-by**: All commits include DCO sign-off (`git commit -s`)
- [ ] **GPG signature**: Commits are GPG-signed (recommended)
- [ ] **License compatibility**: New dependencies are license-compatible
- [ ] **Original work**: I have the right to submit this code
- [ ] **No copyright violations**: Code does not violate third-party rights

### Workflow and CI
- [ ] All CI checks pass (`validate-workflows.yml`, `ci.yml`, etc.)
- [ ] No breaking changes to existing workflows (or documented if intentional)
- [ ] Branch is up to date with target branch

## 🧪 Testing Performed

<!-- Describe the testing you performed to verify your changes -->

### Manual Testing
<!-- Detail the steps you took to manually test -->
- [ ] Tested on device/emulator: [specify]
- [ ] Android version(s): [specify]
- [ ] Test scenarios: [describe]

### Automated Testing
<!-- List any new or modified tests -->
- [ ] Unit tests added/updated
- [ ] Integration tests added/updated
- [ ] Test coverage: [specify %]

## 📊 Impact Analysis

### Performance Impact
<!-- Describe any performance implications -->
- [ ] No performance impact
- [ ] Performance improved by: [specify]
- [ ] Performance decreased by: [specify and justify]

### Breaking Changes
<!-- If this introduces breaking changes, describe them -->
- [ ] No breaking changes
- [ ] Breaking changes (describe):

### User Impact
<!-- How will this affect users? -->
- [ ] No user-facing changes
- [ ] User-facing changes (describe):

## 🔍 Code Review Focus Areas

<!-- Help reviewers by highlighting areas that need special attention -->

### Key Changes
1. [File/component]: [What changed and why]
2. [File/component]: [What changed and why]

### Areas Needing Scrutiny
<!-- Point out complex or risky changes -->
- [ ] [Area 1]: [Reason]
- [ ] [Area 2]: [Reason]

## 📸 Screenshots/Videos

<!-- If UI changes, include before/after screenshots or demo videos -->

### Before
<!-- Screenshot or description of previous behavior -->

### After
<!-- Screenshot or description of new behavior -->

## 🔐 Protected Files Modified

<!-- If you modified protected/nuclear files, explain why -->

- [ ] Modified workflow files (`.github/workflows/*.yml`)
- [ ] Modified security files (`.github/SECURITY.md`, `CODEOWNERS`)
- [ ] Modified validation tools (`tools/check_workflows.py`, etc.)
- [ ] Modified governance docs (`.github/CONTRIBUTING.md`)

**Rationale for protected file changes**:
<!-- Provide detailed justification -->

## 📚 Additional Context

<!-- Add any other context, links, or information about the PR -->

## 🤝 Reviewer Guidance

### Best Place to Start
<!-- Where should reviewers start their review? -->

### Estimated Review Time
<!-- How long do you think this will take to review? -->
- [ ] < 15 minutes (trivial change)
- [ ] 15-30 minutes (small change)
- [ ] 30-60 minutes (medium change)
- [ ] > 60 minutes (large change)

## 🚦 Pre-Merge Checklist

<!-- To be verified before merge -->

- [ ] All review comments addressed
- [ ] All CI checks passing
- [ ] Required approvals obtained
- [ ] Merge conflicts resolved
- [ ] Documentation updated
- [ ] Release notes prepared (if applicable)

## 🏷️ Labels

<!-- Suggested labels for this PR (maintainers will apply) -->
<!-- Examples: bug, enhancement, documentation, security, breaking-change -->

---

## 📝 Maintainer Notes

<!-- For maintainer use only -->

### Merge Strategy
- [ ] Squash and merge (default for master)
- [ ] Create merge commit (for release branches)
- [ ] Rebase and merge (special cases only)

### Post-Merge Actions
- [ ] Update release notes
- [ ] Notify users (if significant)
- [ ] Update documentation site
- [ ] Tag release (if applicable)

---

**By submitting this PR, I confirm**:
- ✅ I have read and agree to follow the [Contributing Guidelines](.github/CONTRIBUTING.md)
- ✅ I have read and understand the [Security Policy](.github/SECURITY.md)
- ✅ I certify the origin of my contribution (DCO)
- ✅ I agree my contribution is licensed under the project's license

**PR Template Version**: 1.0  
**Submission Date**: [Auto-filled by GitHub]
