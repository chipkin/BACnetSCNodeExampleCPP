# WebSocket Implementation Plan for BACnet/SC Node

## Overview

This plan covers adding WebSocket (WSS) client support to the BACnet/SC Node example using
**libwebsockets**, wrapped in a helper class `BACnetSCWebsocketClient`.

Since this project is a **Node only** (not a Hub), only **outbound client connections** to a
Hub are required. No server/listener logic is needed.

---

## 1. Dependency: libwebsockets via vcpkg

Install libwebsockets using vcpkg (includes TLS via OpenSSL):

```
vcpkg install libwebsockets:x64-windows
vcpkg integrate install
```

The `.vcxproj` will then need:
- Additional Include Directories: `$(VcpkgRoot)installed\x64-windows\include`
- Additional Library Directories: `$(VcpkgRoot)installed\x64-windows\lib`
- Additional Dependencies: `websockets.lib`, `libssl.lib`, `libcrypto.lib`

---

## 2. New Files

### `BACnetSCWebsocketClient.h`
Declares the `BACnetSCWebsocketClient` helper class.

**Responsibilities:**
- Hold the Hub URI and TLS configuration
- Manage a single outbound WSS connection to the Hub
- Expose a simple API consumed by the BACnet/SC callbacks
- Buffer inbound and outbound BACnet/SC messages
- Run the libwebsockets service loop (called from the main loop)

**Public API:**
```cpp
class BACnetSCWebsocketClient {
public:
    BACnetSCWebsocketClient();
    ~BACnetSCWebsocketClient();

    // Call once before Connect(). Configures the Hub URI and optional TLS cert paths.
    void Configure(const std::string& hubUri,
                   const std::string& caCertPath    = "",
                   const std::string& clientCertPath = "",
                   const std::string& clientKeyPath  = "");

    // Initiate connection to the Hub (non-blocking). Returns true if the
    // connect attempt was started successfully.
    bool Connect();

    // Cleanly disconnect from the Hub.
    void Disconnect();

    // Pump the libwebsockets event loop. Call this on every iteration of
    // the main application loop (replaces blocking lws_service calls).
    void Service();

    // Queue a BACnet/SC message for sending. Returns true if queued.
    // Called from CallbackSendMessage().
    bool Send(const uint8_t* data, uint16_t length);

    // Consume the next available inbound message. Returns the number of
    // bytes written into `buffer`, or 0 if no message is available.
    // Called from CallbackReceiveMessage().
    uint16_t Receive(uint8_t* buffer, uint16_t maxLength);

    // Connection state query
    bool IsConnected() const;
};
```

---

### `BACnetSCWebsocketClient.cpp`
Implementation of `BACnetSCWebsocketClient`.

**Key internals:**
- `lws_context*` — the libwebsockets context (created in `Connect()`, destroyed in destructor)
- `lws*` — the active websocket connection handle
- `std::queue<std::vector<uint8_t>>` — outbound message queue (with LWS pre-padding)
- `std::queue<std::vector<uint8_t>>` — inbound message queue
- Static libwebsockets callback function (`lws_callback_function`) registered for the
  `bacnet-sc` subprotocol, dispatching events to the class instance via `user` pointer
- TLS configured via `lws_client_connect_info` and `lws_context_creation_info`

**libwebsockets events handled:**
| LWS Event | Action |
|---|---|
| `LWS_CALLBACK_CLIENT_ESTABLISHED` | Set connected flag, log connection |
| `LWS_CALLBACK_CLIENT_RECEIVE` | Copy payload into inbound queue |
| `LWS_CALLBACK_CLIENT_WRITEABLE` | Dequeue and write next outbound message |
| `LWS_CALLBACK_CLIENT_CLOSED` | Clear connected flag, log disconnection |
| `LWS_CALLBACK_CLIENT_CONNECTION_ERROR` | Log error, clear connected flag |

---

## 3. Changes to Existing Files

### `BACnetSCNodeExampleCPP.cpp`

| Location | Change |
|---|---|
| Includes | Add `#include "BACnetSCWebsocketClient.h"` |
| Globals | Add `BACnetSCWebsocketClient g_websocketClient;` |
| `main()` | Call `SetupDevice()`, configure and connect the websocket client, then enter the main loop |
| Main loop | Call `fpLoop()` (BACnet Stack loop) and `g_websocketClient.Service()` each iteration |
| `CallbackInitiateWebsocket` | Call `g_websocketClient.Configure(uri, ...)` then `g_websocketClient.Connect()` |
| `CallbackDisconnectWebsocket` | Call `g_websocketClient.Disconnect()` |
| `CallbackSendMessage` | Call `g_websocketClient.Send(message, messageLength)` |
| `CallbackReceiveMessage` | Call `g_websocketClient.Receive(message, maxMessageLength)` |

### `CASBACnetStackExampleDatabase.h` / `.cpp`

- Add Hub URI and TLS cert path fields to `ExampleDatabaseNetworkPort` so configuration
  lives in the database (consistent with existing pattern):

```cpp
class ExampleDatabaseNetworkPort : public ExampleDatabaseBaseObject {
public:
    bool        changesPending;
    uint8_t     vmac[6];
    std::string primaryHubUri;    // e.g. "wss://192.168.1.100:47808"
    std::string caCertPath;       // path to CA certificate (PEM)
    std::string clientCertPath;   // path to client certificate (PEM), optional
    std::string clientKeyPath;    // path to client private key (PEM), optional
};
```

---

## 4. Main Loop Structure (after changes)

```cpp
// In main(), after setup:
while (true) {
    fpLoop();                    // BACnet Stack processes timers, COV, etc.
    g_websocketClient.Service(); // libwebsockets pumps its event loop
    Sleep(1);                    // Yield ~1ms
}
```

---

## 5. BACnet/SC Subprotocol Note

libwebsockets requires the WebSocket subprotocol name to be declared during context
creation. For BACnet/SC the subprotocol is **`"hub"` or `"bacnet-sc"`** depending on the
Hub implementation. The helper class will use `"hub"` (per ASHRAE 135-2020 Addendum bj).

---

## 6. File Summary

| File | Status |
|---|---|
| `BACnetSCWebsocketClient.h` | **New** |
| `BACnetSCWebsocketClient.cpp` | **New** |
| `BACnetSCNodeExampleCPP.cpp` | **Modified** |
| `CASBACnetStackExampleDatabase.h` | **Modified** |
| `CASBACnetStackExampleDatabase.cpp` | **Modified** |
| `BACnetSCNodeExampleCPP.vcxproj` | **Modified** (add vcpkg refs + new files) |

---

## 7. Out of Scope for this Plan

- Message fragmentation reassembly (libwebsockets handles this via `lws_remaining_packet_payload`)
- Certificate generation / PKI setup
- Hub failover / redundancy (secondary Hub URI)

---

## Review Checklist

- [ ] vcpkg is available in this environment
- [ ] Hub URI and cert paths to use for testing are known
- [ ] BACnet/SC subprotocol name confirmed (`"hub"` vs `"bacnet-sc"`)
- [ ] TLS mutual auth (client cert) required, or CA-only validation?
