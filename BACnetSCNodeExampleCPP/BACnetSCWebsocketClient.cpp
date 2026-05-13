/*
 * BACnet SC Node Example C++
 * ----------------------------------------------------------------------------
 * BACnetSCWebsocketClient.cpp
 *
 * Implementation of BACnetSCWebsocketClient using libwebsockets 4.x.
 */

#include "BACnetSCWebsocketClient.h"

#include <libwebsockets.h>
#include <cstdio>
#include <cstring>
#include <string>

// BACnet/SC WebSocket subprotocol name (ASHRAE 135-2020 Addendum bj).
static const char *BACNETSC_PROTOCOL = "hub.bsc.bacnet.org";

// ---------------------------------------------------------------------------
// libwebsockets protocol table entry - one entry plus a null terminator.
// The per-session user data pointer is set to the BACnetSCWebsocketClient
// instance so the static callback can dispatch to the correct object.
// ---------------------------------------------------------------------------
static struct lws_protocols g_protocols[2]; // filled in Connect()

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

BACnetSCWebsocketClient::BACnetSCWebsocketClient()
    : m_context(NULL), m_wsi(NULL), m_connected(false), m_statusCallback(NULL),
      m_hasPendingStatus(false), m_pendingStatusValue(0), m_pendingStatusErrorCode(0),
      m_stopServiceThread(false)
{
}

BACnetSCWebsocketClient::~BACnetSCWebsocketClient()
{
    Disconnect();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void BACnetSCWebsocketClient::SetStatusCallback(WebSocketStatusCallback cb)
{
    m_statusCallback = cb;
}

void BACnetSCWebsocketClient::Configure(const std::string &hubUri,
                                        const std::string &caCertPath,
                                        const std::string &clientCertPath,
                                        const std::string &clientKeyPath,
                                        const std::string &clientKeyPassword)
{
    m_hubUri = hubUri;
    m_caCertPath = caCertPath;
    m_clientCertPath = clientCertPath;
    m_clientKeyPath = clientKeyPath;
    m_clientKeyPassword = clientKeyPassword;
}

bool BACnetSCWebsocketClient::Connect()
{
    if (m_hubUri.empty())
    {
        fprintf(stderr, "BACnetSCWebsocketClient: Hub URI not configured.\n");
        return false;
    }

    // Stop any running service thread before touching the context.
    if (m_serviceThread.joinable())
    {
        m_stopServiceThread = true;
        if (m_context)
        {
            lws_cancel_service(m_context);
        }
        m_serviceThread.join();
        m_stopServiceThread = false;
    }

    // Tear down any leftover context from a previous connection attempt.
    if (m_context)
    {
        lws_context_destroy(m_context);
        m_context = NULL;
    }
    m_wsi = NULL;
    m_connected = false;

    // Build the protocol table.
    memset(g_protocols, 0, sizeof(g_protocols));
    g_protocols[0].name = BACNETSC_PROTOCOL;
    g_protocols[0].callback = BACnetSCWebsocketClient::LwsCallback;
    g_protocols[0].per_session_data_size = 0;
    g_protocols[0].rx_buffer_size = 0; // use default

    // Context creation info.
    struct lws_context_creation_info ctxInfo;
    memset(&ctxInfo, 0, sizeof(ctxInfo));
    ctxInfo.port = CONTEXT_PORT_NO_LISTEN; // client only
    ctxInfo.protocols = g_protocols;
    ctxInfo.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    if (!m_caCertPath.empty())
    {
        ctxInfo.client_ssl_ca_filepath = m_caCertPath.c_str();
    }
    const bool hasClientCertPath = !m_clientCertPath.empty();
    const bool hasClientKeyPath = !m_clientKeyPath.empty();
    if (hasClientCertPath != hasClientKeyPath)
    {
        fprintf(stderr, "BACnetSCWebsocketClient: %s path provided without %s path.\n",
                hasClientCertPath ? "Client certificate" : "Client private key",
                hasClientCertPath ? "Client private key" : "Client certificate");
        return false;
    }
    if (hasClientCertPath)
    {
        ctxInfo.client_ssl_cert_filepath = m_clientCertPath.c_str();
        ctxInfo.client_ssl_private_key_filepath = m_clientKeyPath.c_str();
    }
    if (!m_clientKeyPassword.empty())
    {
        ctxInfo.ssl_private_key_password = m_clientKeyPassword.c_str();
    }

    m_context = lws_create_context(&ctxInfo);
    if (!m_context)
    {
        fprintf(stderr, "BACnetSCWebsocketClient: Failed to create lws context.\n");
        return false;
    }

    // Determine SSL from URI scheme before parsing.
    const bool useSSL = (m_hubUri.rfind("wss://", 0) == 0);

    // Parse the URI.
    std::string host, path;
    int port = 0;
    if (!ParseUri(host, port, path))
    {
        fprintf(stderr, "BACnetSCWebsocketClient: Failed to parse Hub URI: %s\n",
                m_hubUri.c_str());
        lws_context_destroy(m_context);
        m_context = NULL;
        return false;
    }

    // Connect info.
    struct lws_client_connect_info ccInfo;
    memset(&ccInfo, 0, sizeof(ccInfo));
    ccInfo.context = m_context;
    ccInfo.address = host.c_str();
    ccInfo.port = port;
    ccInfo.path = path.c_str();
    ccInfo.host = host.c_str();
    ccInfo.origin = host.c_str();
    ccInfo.protocol = BACNETSC_PROTOCOL;
    ccInfo.ssl_connection = useSSL ? LCCSCF_USE_SSL : 0;
    ccInfo.userdata = this; // passed as user in LwsCallback

    m_wsi = lws_client_connect_via_info(&ccInfo);
    if (!m_wsi)
    {
        fprintf(stderr, "BACnetSCWebsocketClient: lws_client_connect_via_info failed.\n");
        lws_context_destroy(m_context);
        m_context = NULL;
        return false;
    }

    printf("BACnetSCWebsocketClient: Connecting to %s ...\n", m_hubUri.c_str());

    // Start the background service thread.
    m_serviceThread = std::thread(&BACnetSCWebsocketClient::ServiceThreadFunc, this);
    return true;
}

void BACnetSCWebsocketClient::Disconnect()
{
    // Stop the service thread before touching the context.
    if (m_serviceThread.joinable())
    {
        m_stopServiceThread = true;
        if (m_context)
        {
            lws_cancel_service(m_context);
        }
        m_serviceThread.join();
        m_stopServiceThread = false;
    }

    m_wsi = NULL;
    if (m_context)
    {
        lws_context_destroy(m_context);
        m_context = NULL;
    }
    m_connected = false;

    // Flush queues.
    {
        std::lock_guard<std::mutex> lock(m_rxMutex);
        while (!m_rxQueue.empty())
        {
            m_rxQueue.pop();
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_txMutex);
        while (!m_txQueue.empty())
        {
            m_txQueue.pop();
        }
    }
}

void BACnetSCWebsocketClient::ServiceThreadFunc()
{
    while (!m_stopServiceThread)
    {
        if (m_context)
        {
            lws_service(m_context, 50);

            // After lws_service() returns (e.g., woken by lws_cancel_service from
            // Send()), schedule a writable callback if outbound messages are waiting.
            {
                std::lock_guard<std::mutex> lock(m_txMutex);
                if (!m_txQueue.empty() && m_wsi && m_connected)
                {
                    lws_callback_on_writable(m_wsi);
                }
            }
        }
    }
}

void BACnetSCWebsocketClient::Service()
{
    // lws_service() runs on the background service thread (ServiceThreadFunc).
    // Service() on the main thread only delivers any pending status notification,
    // doing so outside the lws callback stack to prevent re-entrant context
    // create/destroy calls.
    bool hasPending = false;
    uint8_t statusValue = 0;
    uint32_t errorCode = 0;
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        if (m_hasPendingStatus)
        {
            hasPending = true;
            statusValue = m_pendingStatusValue;
            errorCode = m_pendingStatusErrorCode;
            m_hasPendingStatus = false;
        }
    }
    if (hasPending && m_statusCallback)
    {
        m_statusCallback(m_hubUri.c_str(),
                         static_cast<uint16_t>(m_hubUri.size()),
                         statusValue,
                         errorCode);
    }
}

bool BACnetSCWebsocketClient::Send(const uint8_t *data, uint16_t length)
{
    if (!m_context)
    {
        return false;
    }

    // Allocate a buffer with LWS_PRE padding followed by the payload.
    std::vector<uint8_t> buf(LWS_PRE + length);
    memcpy(buf.data() + LWS_PRE, data, length);
    {
        std::lock_guard<std::mutex> lock(m_txMutex);
        m_txQueue.push(buf);
    }

    // Wake the service thread so it can schedule a writable callback.
    lws_cancel_service(m_context);
    return true;
}

uint16_t BACnetSCWebsocketClient::Receive(uint8_t *buffer, uint16_t maxLength)
{
    std::lock_guard<std::mutex> lock(m_rxMutex);
    if (m_rxQueue.empty())
    {
        return 0;
    }

    const std::vector<uint8_t> &msg = m_rxQueue.front();
    uint16_t copyLen = (msg.size() < maxLength)
                           ? static_cast<uint16_t>(msg.size())
                           : maxLength;
    memcpy(buffer, msg.data(), copyLen);
    m_rxQueue.pop();
    return copyLen;
}

bool BACnetSCWebsocketClient::IsConnected() const
{
    return m_connected;
}

const std::string &BACnetSCWebsocketClient::GetHubUri() const
{
    return m_hubUri;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool BACnetSCWebsocketClient::ParseUri(std::string &host, int &port,
                                       std::string &path) const
{
    // Expected format: wss://hostname:port/path
    // or               wss://hostname/path  (port defaults to 443)
    const char *uri = m_hubUri.c_str();

    // Strip the scheme.
    int useSSL = 0;
    if (strncmp(uri, "wss://", 6) == 0)
    {
        useSSL = 1;
        uri += 6;
    }
    else if (strncmp(uri, "ws://", 5) == 0)
    {
        uri += 5;
    }
    else
    {
        return false;
    }
    (void)useSSL;

    // Find the path separator.
    const char *slash = strchr(uri, '/');
    std::string hostPort;
    if (slash)
    {
        hostPort = std::string(uri, slash - uri);
        path = std::string(slash);
    }
    else
    {
        hostPort = std::string(uri);
        path = "/";
    }

    // Split host and port.
    size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos)
    {
        host = hostPort.substr(0, colon);
        port = atoi(hostPort.substr(colon + 1).c_str());
    }
    else
    {
        host = hostPort;
        port = 443; // default WSS port
    }

    return !host.empty() && port > 0;
}

// ---------------------------------------------------------------------------
// Static libwebsockets callback
// ---------------------------------------------------------------------------

int BACnetSCWebsocketClient::LwsCallback(struct lws *wsi, enum lws_callback_reasons reason,
                                         void *user, void *in, size_t len)
{
    // Retrieve the instance pointer from the user data stored in the
    // lws_client_connect_info.userdata field.
    BACnetSCWebsocketClient *self =
        static_cast<BACnetSCWebsocketClient *>(lws_wsi_user(wsi));

    if (!self)
    {
        return 0;
    }

    switch (reason)
    {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        self->OnConnected();
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        self->OnReceive(static_cast<const uint8_t *>(in), len);
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        self->OnWriteable();
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        self->OnClosed();
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        self->OnConnectionError(in ? static_cast<const char *>(in) : "(no detail)");
        break;

    default:
        break;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Internal event handlers
// ---------------------------------------------------------------------------

void BACnetSCWebsocketClient::OnConnected()
{
    m_connected = true;
    printf("BACnetSCWebsocketClient: Connected to Hub (%s).\n", m_hubUri.c_str());
    // Queue status notification; delivered by Service() on the main thread.
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_hasPendingStatus = true;
    m_pendingStatusValue = 2; /* WebsocketStatus_Connected */
    m_pendingStatusErrorCode = 0;
}

void BACnetSCWebsocketClient::OnReceive(const uint8_t *data, size_t len)
{
    if (len > 0)
    {
        std::lock_guard<std::mutex> lock(m_rxMutex);
        m_rxQueue.push(std::vector<uint8_t>(data, data + len));
    }
}

void BACnetSCWebsocketClient::OnWriteable()
{
    std::lock_guard<std::mutex> lock(m_txMutex);
    if (m_txQueue.empty())
    {
        return;
    }

    std::vector<uint8_t> &buf = m_txQueue.front();
    // Payload starts after LWS_PRE bytes.
    size_t payloadLen = buf.size() - LWS_PRE;
    int written = lws_write(m_wsi,
                            buf.data() + LWS_PRE,
                            payloadLen,
                            LWS_WRITE_BINARY);
    if (written < 0)
    {
        fprintf(stderr, "BACnetSCWebsocketClient: lws_write failed.\n");
    }
    m_txQueue.pop();

    // If more messages are queued, request another writable callback.
    if (!m_txQueue.empty())
    {
        lws_callback_on_writable(m_wsi);
    }
}

void BACnetSCWebsocketClient::OnClosed()
{
    m_connected = false;
    m_wsi = NULL;
    printf("BACnetSCWebsocketClient: Connection to Hub closed.\n");
    // Queue status notification; delivered by Service() on the main thread.
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_hasPendingStatus = true;
    m_pendingStatusValue = 3; /* WebsocketStatus_Disconnected */
    m_pendingStatusErrorCode = 0;
}

void BACnetSCWebsocketClient::OnConnectionError(const char *detail)
{
    m_connected = false;
    m_wsi = NULL;
    fprintf(stderr, "BACnetSCWebsocketClient: Connection error: %s\n", detail);
    // Queue status notification; delivered by Service() on the main thread.
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_hasPendingStatus = true;
    m_pendingStatusValue = 4; /* WebsocketStatus_Error */
    m_pendingStatusErrorCode = 0;
}
