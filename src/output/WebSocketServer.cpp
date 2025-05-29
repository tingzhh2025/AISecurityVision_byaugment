#include "WebSocketServer.h"

#ifdef HAVE_WEBSOCKETPP

#include <iostream>
#include <chrono>
#include <functional>

#include "../core/Logger.h"
using namespace AISecurityVision;
WebSocketServer::WebSocketServer() {
    // Set logging settings
    m_server.set_access_channels(websocketpp::log::alevel::all);
    m_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

    // Initialize ASIO
    m_server.init_asio();

    // Set message handlers
    m_server.set_open_handler(std::bind(&WebSocketServer::onOpen, this, std::placeholders::_1));
    m_server.set_close_handler(std::bind(&WebSocketServer::onClose, this, std::placeholders::_1));
    m_server.set_message_handler(std::bind(&WebSocketServer::onMessage, this, std::placeholders::_1, std::placeholders::_2));
    m_server.set_validate_handler(std::bind(&WebSocketServer::onValidate, this, std::placeholders::_1));

    // Set reuse address
    m_server.set_reuse_addr(true);

    LOG_INFO() << "[WebSocketServer] WebSocket server initialized";
}

WebSocketServer::~WebSocketServer() {
    stop();
}

bool WebSocketServer::start(int port) {
    if (m_running.load()) {
        LOG_INFO() << "[WebSocketServer] Server already running";
        return true;
    }

    m_port = port;

    try {
        // Listen on specified port
        m_server.listen(port);
        m_server.start_accept();

        m_running.store(true);
        m_serverThread = std::thread(&WebSocketServer::serverThread, this);

        LOG_INFO() << "[WebSocketServer] WebSocket server started on port " << port;
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[WebSocketServer] Failed to start server: " << e.what();
        m_running.store(false);
        return false;
    }
}

void WebSocketServer::stop() {
    if (!m_running.load()) {
        return;
    }

    LOG_INFO() << "[WebSocketServer] Stopping WebSocket server...";

    m_running.store(false);

    try {
        // Close all connections
        {
            std::lock_guard<std::mutex> lock(m_connectionsMutex);
            for (auto hdl : m_connections) {
                try {
                    m_server.close(hdl, websocketpp::close::status::going_away, "Server shutdown");
                } catch (const std::exception& e) {
                    LOG_ERROR() << "[WebSocketServer] Error closing connection: " << e.what();
                }
            }
            m_connections.clear();
        }

        // Stop the server
        m_server.stop();

        // Wait for server thread to finish
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }

        LOG_INFO() << "[WebSocketServer] WebSocket server stopped";

    } catch (const std::exception& e) {
        LOG_ERROR() << "[WebSocketServer] Error during shutdown: " << e.what();
    }
}

bool WebSocketServer::isRunning() const {
    return m_running.load();
}

void WebSocketServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);

    if (m_connections.empty()) {
        return;
    }

    size_t sentCount = 0;
    for (auto hdl : m_connections) {
        try {
            m_server.send(hdl, message, websocketpp::frame::opcode::text);
            sentCount++;
        } catch (const std::exception& e) {
            LOG_ERROR() << "[WebSocketServer] Failed to send message to client: " << e.what();
        }
    }

    m_messagesSent.fetch_add(sentCount);

    if (sentCount > 0) {
        LOG_INFO() << "[WebSocketServer] Broadcasted alarm to " << sentCount << " clients";
    }
}

void WebSocketServer::sendToConnection(connection_hdl hdl, const std::string& message) {
    try {
        m_server.send(hdl, message, websocketpp::frame::opcode::text);
        m_messagesSent.fetch_add(1);
    } catch (const std::exception& e) {
        LOG_ERROR() << "[WebSocketServer] Failed to send message to specific client: " << e.what();
    }
}

size_t WebSocketServer::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    return m_connections.size();
}

std::vector<std::string> WebSocketServer::getConnectedClients() {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);
    std::vector<std::string> clients;

    for (auto hdl : m_connections) {
        clients.push_back(getConnectionInfo(hdl));
    }

    return clients;
}

void WebSocketServer::setMaxConnections(size_t maxConnections) {
    m_maxConnections = maxConnections;
}

void WebSocketServer::setPingInterval(int intervalMs) {
    m_pingInterval = intervalMs;
}

void WebSocketServer::onOpen(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);

    // Check connection limit
    if (m_connections.size() >= m_maxConnections) {
        LOG_INFO() << "[WebSocketServer] Connection limit reached, rejecting new connection";
        try {
            m_server.close(hdl, websocketpp::close::status::try_again_later, "Server full");
        } catch (const std::exception& e) {
            LOG_ERROR() << "[WebSocketServer] Error rejecting connection: " << e.what();
        }
        return;
    }

    m_connections.insert(hdl);
    m_totalConnections.fetch_add(1);

    std::string clientInfo = getConnectionInfo(hdl);
    LOG_INFO() << "[WebSocketServer] Client connected: " << clientInfo
              << " (Total: " << m_connections.size() << ")";

    // Send welcome message
    try {
        std::string welcomeMsg = R"({"type":"welcome","message":"Connected to AI Security Vision alarm stream","timestamp":")" +
                               std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                                   std::chrono::system_clock::now().time_since_epoch()).count()) + "\"}";
        m_server.send(hdl, welcomeMsg, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        LOG_ERROR() << "[WebSocketServer] Failed to send welcome message: " << e.what();
    }
}

void WebSocketServer::onClose(connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);

    std::string clientInfo = getConnectionInfo(hdl);
    m_connections.erase(hdl);

    LOG_INFO() << "[WebSocketServer] Client disconnected: " << clientInfo
              << " (Remaining: " << m_connections.size() << ")";
}

void WebSocketServer::onMessage(connection_hdl hdl, message_ptr msg) {
    // Handle incoming messages (for future use - ping/pong, client commands)
    std::string payload = msg->get_payload();
    LOG_INFO() << "[WebSocketServer] Received message: " << payload;

    // Echo back for testing
    try {
        std::string response = R"({"type":"echo","message":")" + payload + "\"}";
        m_server.send(hdl, response, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        LOG_ERROR() << "[WebSocketServer] Failed to send echo response: " << e.what();
    }
}

bool WebSocketServer::onValidate(connection_hdl hdl) {
    // Validate connection (check origin, authentication, etc.)
    // For now, accept all connections
    return true;
}

void WebSocketServer::serverThread() {
    try {
        LOG_INFO() << "[WebSocketServer] Server thread started";
        m_server.run();
        LOG_INFO() << "[WebSocketServer] Server thread finished";
    } catch (const std::exception& e) {
        LOG_ERROR() << "[WebSocketServer] Server thread error: " << e.what();
    }
}

std::string WebSocketServer::getConnectionInfo(connection_hdl hdl) {
    try {
        auto con = m_server.get_con_from_hdl(hdl);
        return con->get_remote_endpoint();
    } catch (const std::exception& e) {
        return "unknown";
    }
}

void WebSocketServer::cleanupConnections() {
    std::lock_guard<std::mutex> lock(m_connectionsMutex);

    auto it = m_connections.begin();
    while (it != m_connections.end()) {
        try {
            auto con = m_server.get_con_from_hdl(*it);
            if (con->get_state() == websocketpp::session::state::closed) {
                it = m_connections.erase(it);
            } else {
                ++it;
            }
        } catch (const std::exception& e) {
            it = m_connections.erase(it);
        }
    }
}

#endif // HAVE_WEBSOCKETPP
