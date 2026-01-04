# Git Hooks

This directory contains optional Git hooks that can be used to ensure code quality and compilation before committing.

## Available Hooks

### pre-commit

The pre-commit hook runs compilation checks before allowing a commit. This helps ensure that all committed code can compile successfully.

## Installation

To install a hook:

```bash
# Copy the hook to .git/hooks/
cp .git-hooks/pre-commit .git/hooks/pre-commit

# Make it executable
chmod +x .git/hooks/pre-commit
```

## Usage

Once installed, the hook will run automatically on `git commit`. If checks fail, the commit will be prevented.

To bypass the hook temporarily (not recommended):

```bash
git commit --no-verify
```

## What the pre-commit hook checks

- Java version (17+)
- Gradle wrapper availability
- Android SDK configuration
- Build configuration files
- Source files integrity
- Gradle dependencies

## Uninstallation

To remove a hook:

```bash
rm .git/hooks/pre-commit
```

## Note

These hooks are optional and not required for development. They are provided as a convenience to catch issues early before committing code.
