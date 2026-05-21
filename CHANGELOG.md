# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `CallbackGetPropertyBool` — returns the `Archive` property (identifier 3) for File objects.
- `CallbackSetPropertyBool` — allows BACnet clients to write the `Archive` property on File objects per spec.
- `CallbackGetPropertyDate` — returns the `Modification_Date` date component for File objects using `_stat()`. Returns all-0xFF (BACnet unspecified) when the file does not exist on disk.
- `CallbackGetPropertyTime` — returns the `Modification_Date` time component for File objects using `_stat()`. Returns all-0xFF (BACnet unspecified) when the file does not exist on disk.
- `GetFileModificationTime` helper — retrieves file modification time via `_stat()` / `localtime()` on Windows.
- `g_clientKeyPath` global with `--key-path` CLI argument to specify the private key file path at runtime.
- `g_clientKeyPassword` global with `--key-password` CLI argument for encrypted private key support.
- `issuerCertFile2` (instance 2) as an unused issuer certificate slot; all callbacks handle the missing-file case gracefully (empty read, 0 size, unspecified date/time).
- `fpSetBACnetSCCertificateFileObjects` now registers both issuer certificate file instances (`issuerCertFile1` and `issuerCertFile2`).
- `PROPERTY_IDENTIFIER_ARCHIVE`, `SERVICE_ATOMIC_READ_FILE`, `SERVICE_ATOMIC_WRITE_FILE`, and `FILE_ACCESS_METHOD_STREAM` constants added to `CASBACnetStackExampleConstants.h`.
- `archive` field added to `ExampleDatabaseFile` to track the BACnet Archive property state.

### Changed
- File object instances renumbered: 0 = operational certificate, 1 = issuer certificate (primary), 2 = issuer certificate (secondary/unused), 3 = CSR.
- `CallbackInitiateWebsocket` now reads the CA certificate and client certificate paths directly from the File object database entries instead of separate `NetworkPort` fields.
- `ExampleDatabaseNetworkPort` — removed `caCertPath` and `clientCertPath` fields; certificate paths are now sourced from File objects.
- `CallbackReadFile` returns an empty file with EOF gracefully when the file does not exist on disk (supports unused File object slots).
- `CallbackGetPropertyUInt` returns 0 for `File_Size` when the file does not exist on disk.

## [1.0.0] - 2026-05-12

- Initial release of the BACnet/SC Node example application.
