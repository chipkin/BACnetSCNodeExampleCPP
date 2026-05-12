# BACnetSCNodeExampleCPP

A C++ example application demonstrating a BACnet/SC (Secure Connect) Node using the [CAS BACnet Stack](https://www.chipkin.com/cas-bacnet-stack/) and [libwebsockets](https://libwebsockets.org/).

The application connects to a BACnet/SC Hub over a secure WebSocket (WSS) with mutual TLS authentication, registers a BACnet device with an Analog Input object, broadcasts an I-Am on connect, and allows interactive value changes from the keyboard while the main loop runs.

---

## Table of Contents

1. [Requirements](#requirements)
2. [Project Structure](#project-structure)
3. [Implementation Steps](#implementation-steps)
4. [Certificates](#certificates)
5. [Building](#building)
6. [Running](#running)
7. [User Input](#user-input)
8. [Sample Output](#sample-output)
9. [Architecture Notes](#architecture-notes)

---

## Requirements

| Component | Version / Notes |
|---|---|
| Visual Studio | 2022 (v143 toolset), Win32 (x86) |
| CAS BACnet Stack DLL | 5.3.3.0 – place in `BACnetSCNodeExampleCPP/Debug/` |
| vcpkg | x86-windows packages: `libwebsockets`, `openssl` |
| BACnet/SC Hub | Any ASHRAE 135 compliant hub (e.g., Chipkin Hub) listening on `wss://127.0.0.1:4443/bacnet-sc` |

Install the required vcpkg packages:

```powershell
vcpkg install libwebsockets:x86-windows openssl:x86-windows
```

---

## Project Structure

```
BACnetSCNodeExampleCPP/
├── BACnetSCNodeExampleCPP/
│   ├── BACnetSCNodeExampleCPP.cpp      # Main application entry point
│   ├── BACnetSCWebsocketClient.h/.cpp  # libwebsockets WSS client wrapper
│   ├── CASBACnetStackAdapter.h/.cpp    # CAS BACnet Stack function pointer loader
│   ├── CASBACnetStackExampleDatabase.h/.cpp  # In-memory BACnet object database
│   └── BACnetSCNodeExampleCPP.vcxproj # Visual Studio project file
├── exampleCerts/
│   ├── README.md                       # Certificate generation instructions
│   ├── iss-1.pem                       # CA certificate (not committed)
│   ├── opr-389000.pem                  # Client certificate (not committed)
│   └── key-389000.pem                  # Client private key (not committed)
├── submodules/
│   └── cas-bacnet-stack/               # CAS BACnet Stack source
└── README.md
```

---

## Implementation Steps

The following steps were taken to build this example from the CAS BACnet Stack adapter skeleton.

### 1. Add the CAS BACnet Stack submodule

The `cas-bacnet-stack` repository is included as a Git submodule under `submodules/`. Its source files are compiled directly into the project (no pre-built `.lib` link). The adapter layer (`CASBACnetStackAdapter.h/.cpp`) loads the stack DLL at runtime via `LoadBACnetFunctions()`.

### 2. Create `BACnetSCWebsocketClient`

A standalone `BACnetSCWebsocketClient` class wraps libwebsockets 4.x. It is fully decoupled from the BACnet stack — it knows nothing about `CASBACnetStackAdapter.h` and communicates only via a `WebSocketStatusCallback` function pointer typedef.

**Key design decisions:**

- `lws_service()` runs on a **dedicated background thread** (`ServiceThreadFunc`) because since libwebsockets v3.2 the `timeout_ms` parameter is ignored and the call blocks indefinitely until an internal event fires. Running it on the main thread would freeze keyboard input and the BACnet tick.
- Thread safety is achieved with three `std::mutex` instances (`m_rxMutex`, `m_txMutex`, `m_statusMutex`) and `std::atomic<bool>` for `m_connected` and `m_stopServiceThread`.
- `Send()` calls `lws_cancel_service()` to wake the service thread rather than calling `lws_callback_on_writable()` from the wrong thread.
- Status notifications (`Connected`, `Disconnected`, `Error`) are **deferred** to `Service()` on the main thread. Delivering them directly from lws callbacks would cause the BACnet stack to call `CallbackInitiateWebsocket()` re-entrantly, which would call `lws_context_destroy()` from inside `lws_service()` and corrupt OpenSSL global state.

### 3. Configure mutual TLS

The lws context is created with:
- `client_ssl_ca_filepath` — the CA certificate that signed the Hub's certificate.
- `client_ssl_cert_filepath` / `client_ssl_private_key_filepath` — the client certificate and encrypted private key.
- `ssl_private_key_password` — the passphrase for the encrypted key, supplied via the `--key-password` CLI argument.

### 4. Register BACnet/SC callbacks

Three BACnet/SC-specific callbacks are registered with the stack:

| Callback | Purpose |
|---|---|
| `CallbackInitiateWebsocket` | Stack asks the application to open the WSS connection to the Hub. Calls `Configure()`, `SetStatusCallback()`, and `Connect()`. |
| `CallbackDisconnectWebsocket` | Stack asks the application to close the WSS connection. |
| `CallbackBACnetSCStateChange` | Notifies the application of SC state machine transitions (used for diagnostics). |

### 5. Set up the BACnet device and objects

`SetupDevice()` creates:
- **Device** object — instance `389999`, name `"Example BACnet SC Node"`
- **Analog Input** object — instance `1`, name `"Example Analog Input"`, present value `0.0`, units Degrees Celsius (21), with Reliability and COV Increment properties enabled
- **SC Network Port** object — instance `0`, network type `secureConnect`

Services enabled: `ReadPropertyMultiple`, `SubscribeCOV`.

### 6. Configure the Hub Connector

`ConfigureBACnetSC()` calls:
- `fpSetBACnetSCUuid()` — sets a fixed 16-byte device UUID so the Hub can identify this device across reconnections.
- `fpSetBACnetSCHubConnector()` — sets the VMAC (`00:01:02:03:04:05`), primary Hub URI (`wss://127.0.0.1:4443/bacnet-sc`), and no failover hub. This triggers the first `CallbackInitiateWebsocket`.

### 7. Main loop

```
fpTick()                        ← BACnet stack processes received messages and timers
g_websocketClient.Service()     ← Deliver deferred status notifications to the stack
if (connected && !iAmSent)      ← Send I-Am broadcast once on first connect
DoUserInput()                   ← Handle keyboard input
Sleep(0)                        ← Yield the CPU
```

### 8. Project linker configuration

The following settings were added to the Visual Studio project for **Debug|Win32** and **Release|Win32**:

- **Additional Include Directories**: `C:\vcpkg\installed\x86-windows\include`
- **Additional Library Directories**: `C:\vcpkg\installed\x86-windows\debug\lib` (Debug) / `...\lib` (Release)
- **Additional Dependencies**: `websockets.lib;libssl.lib;libcrypto.lib`

---

## Certificates

Certificate files are **not committed** to this repository because they are generated per-machine and tied to a specific Certificate Authority. See [exampleCerts/README.md](exampleCerts/README.md) for step-by-step OpenSSL commands to generate:

| File | Description |
|---|---|
| `exampleCerts/iss-1.pem` | CA / issuer certificate |
| `exampleCerts/opr-389000.pem` | Client (operational) certificate |
| `exampleCerts/key-389000.pem` | Encrypted client private key (PKCS#8) |

The Hub must be configured to trust the same CA.

---

## Building

1. Open `BACnetSCNodeExampleCPP/BACnetSCNodeExampleCPP.sln` in Visual Studio 2022.
2. Select **Debug | Win32**.
3. Build → Build Solution (`Ctrl+Shift+B`).

Or from PowerShell:

```powershell
cd BACnetSCNodeExampleCPP
msbuild BACnetSCNodeExampleCPP.vcxproj /p:Configuration=Debug /p:Platform=Win32
```

Copy the CAS BACnet Stack DLL (`CASBACnetStack.dll`) and any required vcpkg runtime DLLs (`websockets.dll`, `libssl-3.dll`, `libcrypto-3.dll`) into the `Debug/` output directory alongside the executable.

---

## Running

```powershell
cd BACnetSCNodeExampleCPP\Debug
.\BACnetSCNodeExampleCPP.exe --key-password YOUR_KEY_PASSPHRASE
```

**Arguments:**

| Argument | Description |
|---|---|
| `--key-password <passphrase>` | Passphrase for the encrypted client private key (`key-389000.pem`). Required if the key was generated with a passphrase. |

The Hub URI (`wss://127.0.0.1:4443/bacnet-sc`) and certificate paths (`../exampleCerts/`) are hardcoded in `CASBACnetStackExampleDatabase.cpp`. Adjust them there before building if your Hub is on a different address or port.

---

## User Input

While the application is running, the following single-key commands are available. Press the key and then **Enter** (Windows console).

| Key | Action |
|---|---|
| `i` | Increase the Analog Input present value by `1.1` |
| `d` | Decrease the Analog Input present value by `1.3` |
| `t` | Toggle the Analog Input Reliability between `no-fault-detected` (0) and `unreliable-other` (7) |
| `h` | Print the help / key-binding summary |
| `q` | Quit the application (sends a disconnect to the Hub before exiting) |

When a value is changed with `i` or `d`, `fpValueUpdated()` is called to notify the BACnet stack. Any client that has subscribed to COV on the Analog Input will receive an unsolicited COV notification automatically.

---

## Sample Output

```
CAS BACnet Stack SC Node Example v1.0.0.0
https://github.com/chipkin/BACnetSCNodeExampleCPP

FYI: Loading CAS BACnet Stack functions... OK
FYI: CAS BACnet Stack version: 5.3.3.0
Setting up BACnet device, instance=389999
Adding AnalogInput, instance=1
Adding SC NetworkPort, instance=0
Device setup complete.
Configuring BACnet/SC settings...
Entering main loop...
BACnetSCWebsocketClient: Connecting to wss://127.0.0.1:4443/bacnet-sc ...
BACnetSCWebsocketClient: Connected to Hub (wss://127.0.0.1:4443/bacnet-sc).
Sending I-Am broadcast...
I-Am broadcast sent successfully.
```

After the initial connect, if the Hub rejects the VMAC (e.g., because a previous session used the same VMAC), the stack automatically generates a new random VMAC and reconnects:

```
BACnetSCWebsocketClient: Connection to Hub closed.
BACnetSCWebsocketClient: Connecting to wss://127.0.0.1:4443/bacnet-sc ...
BACnetSCWebsocketClient: Connected to Hub (wss://127.0.0.1:4443/bacnet-sc).
Sending I-Am broadcast...
I-Am broadcast sent successfully.
```

Interactive value changes:

```
i
Increasing Analog Input to 1.100000
d
Decreasing Analog Input to -0.200000
t
h

CAS BACnet Stack SC Node Example v1.0.0.0
https://github.com/chipkin/BACnetSCNodeExampleCPP

Help:
i - (i)ncrease Analog Input by 1.1
d - (d)ecrease Analog Input by 1.3
t - (t)oggle Analog Input Reliability
h - (h)elp
q - (q)uit

q
```

---

## Architecture Notes

- **VMAC all-zeros is not supported by the CAS BACnet Stack.** Passing all-zeros to `fpSetBACnetSCHubConnector()` is explicitly rejected. To get automatic VMAC generation, pass `NULL` or `vmacLength=0` instead. See `BACnetSCHubConnector::SetVmac()` in the stack source for details.
- **libwebsockets v3.2+ ignores `timeout_ms` in `lws_service()`.** The parameter is kept for API compatibility but the call blocks until an event is queued by the internal scheduler. This is why the service loop runs on a background thread.
- **The BACnet SC WebSocket subprotocol name** must be `"hub.bsc.bacnet.org"` as specified in ASHRAE 135 Addendum bj. Any other string causes the Hub to reject the upgrade.
