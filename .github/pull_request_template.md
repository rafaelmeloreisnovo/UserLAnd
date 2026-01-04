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
