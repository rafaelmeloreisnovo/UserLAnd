## Pull Request Checklist

<!-- This template provides a comprehensive checklist for pull requests -->

### 📋 Description

<!-- Provide a clear and concise description of your changes -->

**What does this PR do?**


**Why is this change needed?**


**Related Issues:**
<!-- Link to related issues: Fixes #123, Closes #456 -->


---

### ✅ Pre-submission Checklist

Please ensure all items are checked before submitting:

#### Code Quality
- [ ] Code follows the project's style guidelines (see `.github/CONTRIBUTING.md`)
- [ ] All tests pass locally (`./gradlew test`)
- [ ] Code compiles without errors (`./gradlew assembleDebug`)
- [ ] Added/updated tests for changes (if applicable)
- [ ] Documentation updated (if applicable)
- [ ] No compiler warnings introduced

#### Security & Compliance
- [ ] Commits are signed off with DCO (`git commit -s`)
- [ ] No hardcoded secrets, API keys, or credentials
- [ ] No sensitive data in logs or error messages
- [ ] Dependencies are up-to-date and scanned for vulnerabilities
- [ ] Security implications reviewed and documented
- [ ] Input validation added where applicable

#### CI/CD Checks
- [ ] All CI checks pass (or failures explained below)
- [ ] `validate-workflows` passes (if modifying workflows)
- [ ] `ci` matrix builds pass
- [ ] CodeQL scan passes
- [ ] No new security alerts introduced

#### Protected Files
If this PR modifies protected files (workflows, security configs, CODEOWNERS):
- [ ] Changes are necessary and justified
- [ ] Detailed explanation provided below
- [ ] Maintains existing security controls
- [ ] Does not bypass existing checks
- [ ] Reviewed by @rafaelmeloreisnovo (required for merge)

---

### 🔍 Type of Change

<!-- Check all that apply -->

- [ ] 🐛 Bug fix (non-breaking change fixing an issue)
- [ ] ✨ New feature (non-breaking change adding functionality)
- [ ] 💥 Breaking change (fix or feature causing existing functionality to change)
- [ ] 📝 Documentation update
- [ ] 🎨 Code style/refactoring (no functional changes)
- [ ] 🔧 Build/CI configuration changes
- [ ] 🧪 Test updates
- [ ] ⚡ Performance improvement
- [ ] 🔒 Security fix

---

### 🧪 Testing

**How has this been tested?**
<!-- Describe the tests you ran to verify your changes -->


**Test Configuration:**
- Device/Emulator:
- Android Version:
- Build variant:

---

### 📸 Screenshots (if applicable)

<!-- Add screenshots or screen recordings if your changes affect the UI -->


---

### 🔐 Security Considerations

<!-- If this PR has security implications, describe them here -->


**Does this PR introduce any security risks?**
- [ ] No
- [ ] Yes (explain below)

**If yes, describe mitigation:**


---

### 📊 Impact Analysis

**What components does this PR affect?**
<!-- Check all that apply -->
- [ ] Android application code
- [ ] Native C/C++ code
- [ ] Python tools/scripts
- [ ] Shell scripts
- [ ] GitHub Actions workflows
- [ ] Documentation
- [ ] Build configuration
- [ ] Dependencies

**Potential side effects:**
<!-- List any potential side effects or areas that need extra attention -->


---

### 📚 Additional Context

<!-- Add any other context, design decisions, or implementation notes -->


---

### 👀 Reviewer Guidance

**What should reviewers focus on?**


**Are there any areas of concern?**


---

### 🏷️ Labels

<!-- Maintainers will add appropriate labels -->
<!-- Suggested labels: bug, enhancement, documentation, security, breaking-change -->

---

### ⚖️ Legal & Compliance

By submitting this PR, I confirm that:
- [ ] I have read and agree to the [Code of Conduct](../CODE_OF_CONDUCT.md)
- [ ] I have read and followed the [Contributing Guidelines](.github/CONTRIBUTING.md)
- [ ] I certify that I have the right to submit this contribution (DCO)
- [ ] My contribution is under the same license as this project (see LICENSE)
- [ ] I understand that changes to protected files require maintainer approval

---

### 🔖 Metadata

<!-- Maintainers: DO NOT EDIT -->
- **RAFCODE-Φ Compliance**: Pending verification
- **Ethica[8] Check**: Automated (blocks infinite loops)
- **SBOM Generation**: Triggered on merge
- **Signature Verification**: Required for protected files

---

**Note:** This is a prototype/draft PR template. All changes preserve original authorship (@rafaelmeloreisnovo) and require explicit approval before merge. No automated merges are enabled.
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
