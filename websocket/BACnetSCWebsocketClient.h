/*
 * BACnet SC Node Example C++
 * ----------------------------------------------------------------------------
 * BACnetSCWebsocketClient.h
 * Version: 1.0.0
 *
 * Helper class that wraps libwebsockets to provide a simple outbound WSS
 * client connection to a BACnet/SC Hub. Only the Node (client) role is
 * implemented here - no server/listener logic is present.
 *
 * Usage:
 *   1. Call Configure() with the Hub URI and optional TLS cert paths.
 *   2. Call Connect() to initiate the connection.
 *   3. Call Service() on every iteration of the main loop.
 *   4. Call Send() / Receive() from the BACnet Stack callbacks.
 *   5. Call Disconnect() to cleanly close the connection.
 */

#ifndef __BACnetSCWebsocketClient_h__
#define __BACnetSCWebsocketClient_h__

#include <string>
#include <queue>
#include <vector>
#include <stdint.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <libwebsockets.h>

// Forward-declare libwebsockets pointer types used by this header.
struct lws_context;
struct lws;

// Callback signature matching BACnetStack_SetBACnetSCWebSocketStatus.
// Called by the client when the WebSocket connection state changes so the
// application can forward the status to the BACnet stack without this class
// having any direct dependency on CASBACnetStackAdapter.
typedef void (*WebSocketStatusCallback)(const char *uri, uint16_t uriLength,
                                        uint8_t status, uint32_t errorCode);

static const char BACNET_SC_WEBSOCKET_CLIENT_VERSION[] = "1.0.0";

class BACnetSCWebsocketClient
{
public:
    BACnetSCWebsocketClient();
    ~BACnetSCWebsocketClient();

    // Configure the Hub URI and optional TLS certificate paths.
    // Must be called before Connect().
    //   hubUri        - full WSS URI, e.g. "wss://192.168.1.100:47808"
    //   caCertPath    - path to CA certificate PEM file (empty = no CA pinning)
    //   clientCertPath - path to client certificate PEM file (empty = no mutual TLS)
    //   clientKeyPath  - path to client private key PEM file  (empty = no mutual TLS)
    void Configure(const std::string &hubUri,
                   const std::string &caCertPath = "",
                   const std::string &clientCertPath = "",
                   const std::string &clientKeyPath = "",
                   const std::string &clientKeyPassword = "");

    // Register a callback to receive WebSocket status changes.
    // Pass fpSetBACnetSCWebSocketStatus (or any compatible function) here.
    void SetStatusCallback(WebSocketStatusCallback cb);

    // Initiate the outbound WSS connection to the Hub (non-blocking).
    // Returns true if the attempt was started successfully.
    bool Connect();

    // Cleanly disconnect from the Hub and destroy the libwebsockets context.
    void Disconnect();

    // Pump the libwebsockets event loop. Call once per main-loop iteration.
    void Service();

    // Queue a BACnet/SC message for sending. Returns true if queued.
    // Called from CallbackSendMessage().
    bool Send(const uint8_t *data, uint16_t length);

    // Consume the next available inbound message into buffer.
    // Returns the number of bytes written, or 0 if no message is available.
    // Called from CallbackReceiveMessage().
    uint16_t Receive(uint8_t *buffer, uint16_t maxLength);

    // Returns true when the WebSocket handshake has completed successfully.
    bool IsConnected() const;

    // Returns the Hub URI this client is configured to connect to.
    const std::string &GetHubUri() const;

private:
    // libwebsockets static callback - dispatches to the instance via user data.
    static int LwsCallback(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len);

    // Internal event handlers called from LwsCallback.
    void OnConnected();
    void OnReceive(const uint8_t *data, size_t len);
    void OnWriteable();
    void OnClosed();
    void OnConnectionError(const char *detail);

    // Parse m_hubUri into host, port and path components.
    bool ParseUri(std::string &host, int &port, std::string &path) const;

    // Entry point for the background thread that owns the lws_service() loop.
    void ServiceThreadFunc();

    // libwebsockets context and active connection.
    lws_context *m_context;
    lws *m_wsi;

    // Connection configuration.
    std::string m_hubUri;
    std::string m_caCertPath;
    std::string m_clientCertPath;
    std::string m_clientKeyPath;
    std::string m_clientKeyPassword;

    // Connection state (atomic: written by service thread, read by main thread).
    std::atomic<bool> m_connected;

    // Optional callback invoked on connection state changes.
    WebSocketStatusCallback m_statusCallback;

    // Pending status notification delivered by Service() on the main thread.
    // Written by lws callbacks (service thread), read/cleared by Service() (main thread).
    bool m_hasPendingStatus;
    uint8_t m_pendingStatusValue;
    uint32_t m_pendingStatusErrorCode;

    // Background thread that owns the lws_service() loop.
    std::thread m_serviceThread;
    std::atomic<bool> m_stopServiceThread;

    // Mutexes protecting cross-thread access to queues and status fields.
    std::mutex m_rxMutex;
    std::mutex m_txMutex;
    std::mutex m_statusMutex;

    // Inbound message queue: each entry is one complete BACnet/SC message.
    std::queue<std::vector<uint8_t>> m_rxQueue;

    // Outbound message queue: each entry includes LWS_PRE padding bytes
    // followed by the message payload, ready for lws_write().
    std::queue<std::vector<uint8_t>> m_txQueue;
};

#endif // __BACnetSCWebsocketClient_h__
