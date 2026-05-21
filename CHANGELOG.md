# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.0] - 2026-05-21

### Added
- **File Object property support** — BACnet clients can now read and write the `Archive` property on all File objects, and `Modification_Date` is reported from the actual file timestamp on disk.
- **Second issuer certificate slot** — A second issuer certificate File object (instance 2) is now registered with the BACnet/SC stack. The slot is optional; if the file does not exist on disk, all File properties return valid BACnet "unspecified" values rather than errors.
- **`--key-path` command-line argument** — The client private key file path can now be overridden at launch instead of being hard-coded.
- **`--key-password` command-line argument** — A passphrase for an encrypted private key can now be supplied at launch, enabling use of password-protected key files.

### Changed
- Certificate file paths are now managed entirely through the File object database. The Network Port object no longer holds separate CA certificate or client certificate path fields.
- File object instances have been renumbered for consistency: 0 = operational certificate, 1 = issuer certificate (primary), 2 = issuer certificate (secondary), 3 = certificate signing request.

## [1.0.0] - 2026-05-12

- Initial release of the BACnet/SC Node example application.
