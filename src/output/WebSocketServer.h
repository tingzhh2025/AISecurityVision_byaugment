#pragma once

#ifdef HAVE_WEBSOCKETPP

#include <string>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>

// WebSocket++ includes
#ifdef ASIO_STANDALONE
#include <websocketpp/config/asio_no_tls.hpp>
#else
#include <websocketpp/config/asio_no_tls.hpp>
#endif

#include <websocketpp/server.hpp>

/**
 * @brief WebSocket server for real-time alarm streaming
 *
 * This class provides:
 * - WebSocket server for persistent connections
 * - Real-time alarm broadcasting to connected clients
 * - Connection management and health monitoring
 * - Thread-safe message broadcasting
 */
class WebSocketServer {
public:
    using server = websocketpp::server<websocketpp::config::asio>;
    using connection_hdl = websocketpp::connection_hdl;
    using message_ptr = server::message_ptr;

    WebSocketServer();
    ~WebSocketServer();

    // Server control
    bool start(int port);
    void stop();
    bool isRunning() const;

    // Message broadcasting
    void broadcast(const std::string& message);
    void sendToConnection(connection_hdl hdl, const std::string& message);

    // Connection management
    size_t getConnectionCount() const;
    std::vector<std::string> getConnectedClients();

    // Configuration
    void setMaxConnections(size_t maxConnections);
    void setPingInterval(int intervalMs);

private:
    // WebSocket event handlers
    void onOpen(connection_hdl hdl);
    void onClose(connection_hdl hdl);
    void onMessage(connection_hdl hdl, message_ptr msg);
    bool onValidate(connection_hdl hdl);

    // Server thread
    void serverThread();

    // Utility methods
    std::string getConnectionInfo(connection_hdl hdl);
    void cleanupConnections();

    // Member variables
    server m_server;
    std::set<connection_hdl, std::owner_less<connection_hdl>> m_connections;
    mutable std::mutex m_connectionsMutex;

    std::thread m_serverThread;
    std::atomic<bool> m_running{false};

    int m_port{8081};
    size_t m_maxConnections{100};
    int m_pingInterval{30000};  // 30 seconds

    // Statistics
    std::atomic<size_t> m_totalConnections{0};
    std::atomic<size_t> m_messagesSent{0};
};

#endif // HAVE_WEBSOCKETPP
