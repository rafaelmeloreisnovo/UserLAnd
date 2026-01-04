<!-- 
Pull Request Template - RAFAELIA Enhanced
Author: @rafaelmeloreisnovo
RAFCODE-Φ seal: PROTOTYPE-DRAFT-v0.1.0
Status: Draft - requires maintainer approval
-->

## Pull Request Template

**⚠️ IMPORTANT:** This PR template includes RAFAELIA governance features (PROTOTYPE/DRAFT). All changes require explicit maintainer approval before merge.

---

## Summary

<!-- Provide a brief description of the changes in this PR -->

**What changed:**

**Why:**

**Issue(s):** Fixes #

---

## Type of Change

Please check the relevant options:

- [ ] 🐛 Bug fix (non-breaking change that fixes an issue)
- [ ] ✨ New feature (non-breaking change that adds functionality)
- [ ] 💥 Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] 📝 Documentation update
- [ ] 🔧 Configuration change
- [ ] 🔐 Security fix
- [ ] ♻️ Refactoring (no functional changes)
- [ ] 🧪 Test update
- [ ] 🚀 Performance improvement

---

## Testing Performed

<!-- Describe the testing you performed to verify your changes -->

**Manual Testing:**
- [ ] Tested on Android device/emulator
- [ ] Tested relevant user flows
- [ ] Tested edge cases

**Automated Testing:**
- [ ] Unit tests pass (`./gradlew test`)
- [ ] Integration tests pass (if applicable)
- [ ] Lint checks pass (`./gradlew ktlint`)

**Test Environment:**
- Android Version:
- Device/Emulator:
- UserLAnd Version:

**Test Results:**
<!-- Describe what you tested and the results -->

---

## Code Quality Checklist

- [ ] My code follows the project's style guidelines
- [ ] I have performed a self-review of my code
- [ ] I have commented my code where necessary
- [ ] My changes generate no new warnings
- [ ] I have updated the documentation accordingly
- [ ] My changes maintain backward compatibility (or breaking changes are documented)

---

## Architecture Compliance

- [ ] Changes follow MVVM-C architecture
- [ ] UI updates are in XML/view controllers only
- [ ] Business logic is decoupled from Android framework
- [ ] Code is organized in appropriate packages (model/utils/viewmodel)

---

## Security Checklist (PROTOTYPE)

**⚠️ RAFCODE-Φ security requirements (pending approval):**

- [ ] No hardcoded secrets or credentials
- [ ] No SQL injection vulnerabilities
- [ ] No path traversal vulnerabilities
- [ ] Input validation implemented where needed
- [ ] Security implications considered and documented
- [ ] No infinite loops (Ethica[8] compliant)

---

## CI/CD Status (PROTOTYPE)

**Required status checks (pending approval):**

- [ ] ✅ `validate-workflows` - Workflow validation passed
- [ ] ✅ `ci` - CI matrix tests passed
- [ ] ✅ Code review approved by maintainer
- [ ] ✅ Commits are signed (DCO)
- [ ] ✅ Documentation updated

---

## Reviewer Guidance

**Best place to start reviewing:**
<!-- Tell reviewers where to start in the diff -->

**Areas needing extra attention:**
<!-- Highlight complex or critical changes -->

**Known limitations:**
<!-- Document any known issues or limitations -->

---

## User Impact

**Value to users:**
<!-- Explain how this change benefits users -->

**Migration required:**
<!-- Describe any migration steps users need to take -->

**Breaking changes:**
<!-- List any breaking changes and migration path -->

---

## Additional Context

<!-- Add any other context, screenshots, or information about the PR -->

**Screenshots/Videos:**
<!-- If applicable, add visual evidence of changes -->

**Related PRs:**
<!-- Link to related pull requests -->

**External References:**
<!-- Link to external documentation, issues, or discussions -->

---

## RAFAELIA Metadata (PROTOTYPE)

**⚠️ This section is for RAFAELIA-enhanced PRs:**

- **Author:** @rafaelmeloreisnovo
- **RAFCODE-Φ Seal:** PROTOTYPE-DRAFT-v0.1.0
- **Ethica[8] Compliant:** ✅ Yes / ❌ No
- **Requires Maintainer Approval:** ✅ YES
- **Auto-merge Allowed:** ❌ NO

**Authorship Preservation:**
All artifacts in this PR preserve authorship attribution and include RAFCODE-Φ seals. No automatic execution of infinite loops. Ethica[8] blocks prohibited executions.

**Governance Note:**
This PR is in DRAFT status until reviewed for legal/ethical compliance and approved by repository maintainer. **DO NOT MERGE** without explicit maintainer approval.

---

## Merge Checklist (for maintainers)

- [ ] All status checks passed
- [ ] Code review completed and approved
- [ ] Documentation is up to date
- [ ] CHANGELOG.md updated (if applicable)
- [ ] Security implications reviewed
- [ ] Breaking changes documented
- [ ] Maintainer explicitly approves merge
- [ ] PR removed from draft status (if applicable)

---

**Merge Strategy:**
- [ ] Squash and merge (for feature branches to master)
- [ ] Create merge commit (for release branches)
- [ ] Rebase and merge (if linear history preferred)

---

<!-- 
RAFCODE-Φ seal: @rafaelmeloreisnovo
Status: PROTOTYPE-DRAFT
DO NOT MERGE without explicit maintainer approval
All features require human review before activation
-->
