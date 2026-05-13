# BACnetSCWebsocketClient

**Version: 1.0.0**

A lightweight libwebsockets-based outbound WSS client for BACnet/SC Node connectivity.

## Overview

`BACnetSCWebsocketClient` wraps [libwebsockets](https://libwebsockets.org/) to provide a
simple outbound WebSocket Secure (WSS) connection from a BACnet/SC Node to a BACnet/SC Hub.

This component implements the **Node (client) role only** — no Hub/server logic is present.

## Files

| File | Description |
|---|---|
| `BACnetSCWebsocketClient.h` | Class declaration and public API |
| `BACnetSCWebsocketClient.cpp` | Implementation using libwebsockets 4.x |
| `WEBSOCKET_IMPLEMENTATION_PLAN.md` | Original design and implementation plan |
| `CHANGELOG.md` | Version history |

## Dependencies

- **libwebsockets** 4.x (via vcpkg `libwebsockets:x86-windows`)
- **OpenSSL** (included with libwebsockets via vcpkg)

## Public API

```cpp
// Configure the Hub URI and optional mutual TLS cert paths.
void Configure(const std::string& hubUri,
               const std::string& caCertPath     = "",
               const std::string& clientCertPath = "",
               const std::string& clientKeyPath  = "",
               const std::string& clientKeyPassword = "");

// Set a callback to be notified of connection state changes.
void SetStatusCallback(WebSocketStatusCallback cb);

// Initiate the outbound connection. Starts the background service thread.
bool Connect();

// Cleanly close the connection and stop the service thread.
void Disconnect();

// Deliver pending status notifications (call on every main loop iteration).
void Service();

// Queue a message for sending. Thread-safe.
bool Send(const uint8_t* data, uint16_t length);

// Consume the next received message. Returns bytes copied, or 0 if empty.
uint16_t Receive(uint8_t* buffer, uint16_t maxLength);

// Returns true if the WebSocket connection is currently established.
bool IsConnected() const;

// Returns the configured Hub URI.
const std::string& GetHubUri() const;
```

## Threading Model

The libwebsockets service loop runs on a dedicated background thread started by `Connect()`.
`Service()` must be called on the main thread each loop iteration — it delivers deferred
connection-state callbacks safely outside the libwebsockets callback stack.

## TLS / Certificates

Mutual TLS is supported. Provide paths to:
- CA certificate (PEM) — used to verify the Hub's server certificate
- Client certificate (PEM) — this Node's identity certificate
- Client private key (PEM) — may be encrypted; supply the passphrase via `Configure()`

See [`../exampleCerts/README.md`](../exampleCerts/README.md) for certificate generation instructions.

## BACnet/SC Subprotocol

Uses WebSocket subprotocol `"hub.bsc.bacnet.org"` per ASHRAE 135-2020 Addendum bj.
