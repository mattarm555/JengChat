#include "networking.h"
#include "config.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <atomic>
#include <mutex>
#include <queue>
#include <deque>
#include <thread>
#include <chrono>
#include <algorithm>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
#define CLOSE_SOCKET closesocket
#define INVALID_SOCK INVALID_SOCKET
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
using Socket = int;
#define CLOSE_SOCKET close
#define INVALID_SOCK -1
#endif

namespace {
std::atomic<bool> running{false}, connected{false}, connecting{false};
std::thread worker;
std::mutex stateMutex;
std::queue<NetMessage> messages;
std::deque<std::string> outgoing;
size_t queuedBytes = 0;
std::string lastError;
void Error(const std::string& text) { std::lock_guard<std::mutex> lock(stateMutex); lastError = text; }
void Push(const std::string& line) {
    auto split = line.find('|');
    std::lock_guard<std::mutex> lock(stateMutex);
    if (messages.size() >= 4096) { running = false; lastError = "Too many incoming messages."; return; }
    messages.push({split == std::string::npos ? "RAW" : line.substr(0, split),
        split == std::string::npos ? line : line.substr(split + 1)});
}
bool Nonblocking(Socket socket) {
#ifdef _WIN32
    u_long one = 1; return ioctlsocket(socket, FIONBIO, &one) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}
bool Retry(SSL* ssl, int result) {
    int error = SSL_get_error(ssl, result);
    return error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE;
}
std::string Hex(const std::string& value) {
    static const char digits[] = "0123456789abcdef";
    std::string encoded;
    for (unsigned char c : value) { encoded += digits[c >> 4]; encoded += digits[c & 15]; }
    return encoded;
}
void Run(std::string host, int port, std::string username, std::string password, bool create) {
    Socket socketHandle = INVALID_SOCK;
    SSL_CTX* context = nullptr;
    SSL* tls = nullptr;
    std::string pendingWrite, pendingRead;
    auto cleanup = [&] {
        OPENSSL_cleanse(password.data(), password.size());
        OPENSSL_cleanse(pendingWrite.data(), pendingWrite.size());
        if (tls) SSL_free(tls);
        if (context) SSL_CTX_free(context);
        if (socketHandle != INVALID_SOCK) CLOSE_SOCKET(socketHandle);
        bool wasConnected = connected.exchange(false);
        connecting = false; running = false;
        if (wasConnected) Push("ERR|Disconnected from server. Please sign in again.");
    };
    // The current deployment uses an IPv4 literal, avoiding blocking DNS lookup.
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        Error("Server address must be an IPv4 address."); cleanup(); return;
    }
    socketHandle = socket(AF_INET, SOCK_STREAM, 0);
    if (socketHandle == INVALID_SOCK || !Nonblocking(socketHandle)) {
        Error("Cannot create network connection."); cleanup(); return;
    }
    int noDelay = 1;
    setsockopt(socketHandle, IPPROTO_TCP, TCP_NODELAY, (const char*)&noDelay, sizeof(noDelay));
    connect(socketHandle, (sockaddr*)&address, sizeof(address));
    bool tcpReady = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (running && std::chrono::steady_clock::now() < deadline) {
        fd_set writes; FD_ZERO(&writes); FD_SET(socketHandle, &writes);
        timeval timeout{0, 20000};
        int ready = select((int)socketHandle + 1, nullptr, &writes, nullptr, &timeout);
        if (ready > 0) {
            int error = 0; socklen_t length = sizeof(error);
            if (getsockopt(socketHandle, SOL_SOCKET, SO_ERROR, (char*)&error, &length) == 0 && error == 0) tcpReady = true;
            break;
        }
        if (ready < 0) break;
    }
    if (!running || !tcpReady) { Error("Could not connect to server."); cleanup(); return; }
    context = SSL_CTX_new(TLS_client_method());
    if (!context) { Error("Unable to initialize encryption."); cleanup(); return; }
    SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
    SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);
    SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
    // Only the explicitly distributed server certificate is trusted. Never fall back to plaintext.
    if (SSL_CTX_load_verify_locations(context, SERVER_CERTIFICATE, nullptr) != 1) {
        Error("Server certificate is missing. Install assets/security/server.crt."); cleanup(); return;
    }
    tls = SSL_new(context);
    if (!tls || SSL_set_fd(tls, (int)socketHandle) != 1 ||
        X509_VERIFY_PARAM_set1_ip_asc(SSL_get0_param(tls), host.c_str()) != 1) {
        Error("Unable to configure certificate verification."); cleanup(); return;
    }
    bool tlsReady = false;
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (running && std::chrono::steady_clock::now() < deadline) {
        ERR_clear_error(); int result = SSL_connect(tls);
        if (result == 1) { tlsReady = true; break; }
        if (!Retry(tls, result)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!tlsReady || SSL_get_verify_result(tls) != X509_V_OK) {
        Error("Secure connection failed. Check that the updated TLS server is running and that its certificate, IP address, and date match."); cleanup(); return;
    }
    pendingWrite = std::string(create ? "AUTH_REGISTER|" : "AUTH_LOGIN|") + username + "|" + Hex(password) + "\n";
    OPENSSL_cleanse(password.data(), password.size()); password.clear();
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    auto writeStarted = std::chrono::steady_clock::now();
    bool failed = false;
    while (running && !failed) {
        if (!connected && std::chrono::steady_clock::now() > deadline) {
            Error("Sign-in timed out. Please retry."); break;
        }
        if (pendingWrite.empty()) {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (!outgoing.empty()) {
                pendingWrite = std::move(outgoing.front()); outgoing.pop_front();
                queuedBytes -= pendingWrite.size(); writeStarted = std::chrono::steady_clock::now();
            }
        }
        if (!pendingWrite.empty()) {
            ERR_clear_error(); int sent = SSL_write(tls, pendingWrite.data(), (int)pendingWrite.size());
            if (sent <= 0) {
                if (!Retry(tls, sent) || std::chrono::steady_clock::now() - writeStarted > std::chrono::seconds(10)) {
                    Error("Connection interrupted while sending."); break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(4)); continue;
            }
            OPENSSL_cleanse(pendingWrite.data(), pendingWrite.size()); pendingWrite.clear();
        }
        char buffer[8192];
        ERR_clear_error(); int received = SSL_read(tls, buffer, sizeof(buffer));
        if (received > 0) {
            pendingRead.append(buffer, received);
            if (pendingRead.size() > 262144) { Error("Server message too large."); break; }
            size_t end;
            while ((end = pendingRead.find('\n')) != std::string::npos) {
                std::string line = pendingRead.substr(0, end); pendingRead.erase(0, end + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!connected) {
                    if (line.rfind("AUTH_ERROR|", 0) == 0) { Error(line.substr(11)); failed = true; break; }
                    if (line.rfind("AUTH_OK|", 0) != 0) continue;
                    connected = true;
                }
                Push(line);
                if (connected) connecting = false;
            }
        } else if (!Retry(tls, received)) { Error("Connection closed by server."); break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
    cleanup();
}
}

bool NetConnect(const std::string& host, int port, const std::string& username,
    const std::string& password, bool createAccount) {
    NetDisconnect();
#ifdef _WIN32
    static bool initialized = false;
    if (!initialized) { WSADATA data; if (WSAStartup(MAKEWORD(2,2), &data) != 0) { Error("Winsock initialization failed."); return false; } initialized = true; }
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        lastError.clear(); while (!messages.empty()) messages.pop();
        outgoing.clear(); queuedBytes = 0;
    }
    running = true; connecting = true;
    worker = std::thread(Run, host, port, username, password, createAccount);
    return true;
}
void NetDisconnect() {
    running = false;
    if (worker.joinable()) worker.join();
    connected = false; connecting = false;
}
bool NetIsConnected() { return connected; }
bool NetIsConnecting() { return connecting; }
std::string NetLastError() { std::lock_guard<std::mutex> lock(stateMutex); return lastError; }
bool NetSendLine(const std::string& line) {
    if (!connected) return false;
    if (line.size() > 4096 || line.find_first_of("\r\n") != std::string::npos) return false;
    std::lock_guard<std::mutex> lock(stateMutex);
    if (queuedBytes + line.size() + 1 > 262144) { lastError = "Connection busy. Please retry."; return false; }
    outgoing.push_back(line + "\n"); queuedBytes += line.size() + 1;
    return true;
}
bool NetChangeUsername(const std::string& newUsername, const std::string& currentPassword) {
    if (newUsername.find_first_of("|\r\n") != std::string::npos) return false;
    std::string encoded = Hex(currentPassword);
    std::string request = "ACCOUNT_USERNAME|" + newUsername + "|" + encoded;
    bool sent = NetSendLine(request);
    OPENSSL_cleanse(encoded.data(), encoded.size());
    OPENSSL_cleanse(request.data(), request.size());
    return sent;
}
bool NetChangePassword(const std::string& currentPassword, const std::string& newPassword) {
    std::string currentEncoded = Hex(currentPassword);
    std::string newEncoded = Hex(newPassword);
    std::string request = "ACCOUNT_PASSWORD|" + currentEncoded + "|" + newEncoded;
    bool sent = NetSendLine(request);
    OPENSSL_cleanse(currentEncoded.data(), currentEncoded.size());
    OPENSSL_cleanse(newEncoded.data(), newEncoded.size());
    OPENSSL_cleanse(request.data(), request.size());
    return sent;
}
bool NetDeleteAccount(const std::string& currentPassword) {
    std::string encoded = Hex(currentPassword);
    std::string request = "ACCOUNT_DELETE|" + encoded;
    bool sent = NetSendLine(request);
    OPENSSL_cleanse(encoded.data(), encoded.size());
    OPENSSL_cleanse(request.data(), request.size());
    return sent;
}
std::vector<NetMessage> NetPollMessages() {
    std::lock_guard<std::mutex> lock(stateMutex);
    std::vector<NetMessage> result;
    while (!messages.empty()) { result.push_back(std::move(messages.front())); messages.pop(); }
    return result;
}
