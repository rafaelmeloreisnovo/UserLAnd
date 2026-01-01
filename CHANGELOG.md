# Changelog

All notable changes to the UserLAnd project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- CHANGELOG.md to track project changes systematically
- Documentation improvements for build process and troubleshooting
- Enhanced CI/CD workflow documentation

### Changed
- Updated Kotlin version to 2.0.0 for improved performance and features
- Android Gradle Plugin updated to 8.6.0
- Navigation component updated to 2.9.6

### Deprecated
- Legacy Sentry API (migration to 5.x+ API in progress)
- BillingManager deprecated APIs (queryPurchases, Purchase.sku)

### Security
- Project uses AGP 8.x with AndroidManifest package attribute removed
- ViewBinding migration completed for improved type safety

## [2.8.3] - Current Release

### Features
- Full Linux distribution support on Android without root
- Support for multiple architectures (arm64-v8a, armeabi-v7a, x86, x86_64)
- VNC and SSH session support
- RAFAELIA telemetry and monitoring system integration
- Static musl/clang utilities for system monitoring

### Technical
- Target SDK: 35 (Android 15)
- Minimum SDK: 23 (Android 6.0)
- Build Tools: 34.0.0
- NDK: 27.3.13750724 (configurable)

### Infrastructure
- GitHub Actions CI/CD pipeline
- Multiple workflow types (debug, release, architecture-specific builds)
- Automated APK building and signing
- Artifact retention and deployment

## Development Guidelines

### Version Numbering
Version codes are generated as time since June 1, 2019, allowing for sequential deployments to both production and beta tracks.

### Building
See [README.md](README.md) for detailed build instructions.

### Contributing
See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines.

---

[Unreleased]: https://github.com/CypherpunkArmory/UserLAnd/compare/v2.8.3...HEAD
[2.8.3]: https://github.com/CypherpunkArmory/UserLAnd/releases/tag/v2.8.3
