#include "networking.h"

#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
#define CLOSE_SOCKET closesocket
#define INVALID_SOCK INVALID_SOCKET
#define SHUT_BOTH SD_BOTH
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
using Socket = int;
#define CLOSE_SOCKET close
#define INVALID_SOCK -1
#define SHUT_BOTH SHUT_RDWR
#endif

using namespace std;

static Socket gSocket = INVALID_SOCK;
static atomic<bool> gConnected(false);
static atomic<bool> gRunning(false);
static thread gReceiver;
static mutex gQueueMutex;
static queue<NetMessage> gMessages;
static mutex gErrorMutex;
static string gLastError;

static void SetError(const string& text)
{
    lock_guard<mutex> lock(gErrorMutex);
    gLastError = text;
}

string NetLastError()
{
    lock_guard<mutex> lock(gErrorMutex);
    return gLastError;
}

static bool SendAll(Socket socketHandle, const string& data)
{
    int total = 0;
    while (total < (int)data.size())
    {
        int sent = (int)send(socketHandle, data.c_str() + total,
                             (int)data.size() - total, 0);
        if (sent <= 0) return false;
        total += sent;
    }
    return true;
}

static void PushMessage(const string& line)
{
    NetMessage msg;
    size_t split = line.find('|');
    if (split == string::npos)
    {
        msg.type = "RAW";
        msg.data = line;
    }
    else
    {
        msg.type = line.substr(0, split);
        msg.data = line.substr(split + 1);
    }

    lock_guard<mutex> lock(gQueueMutex);
    gMessages.push(msg);
}

static void ReceiverLoop()
{
    constexpr int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    string pending;

    while (gRunning)
    {
        int received = (int)recv(gSocket, buffer, BUFFER_SIZE, 0);
        if (received <= 0) break;

        pending.append(buffer, received);

        while (true)
        {
            size_t newline = pending.find('\n');
            if (newline == string::npos) break;

            string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            PushMessage(line);
        }
    }

    bool wasConnected = gConnected.exchange(false);
    gRunning = false;
    if (wasConnected) PushMessage("ERR|Disconnected from server.");
}

bool NetConnect(const string& host, int port, const string& username)
{
    if (gConnected) return true;
    SetError("");

#ifdef _WIN32
    static atomic<bool> winsockReady(false);
    if (!winsockReady)
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            SetError("WSAStartup failed.");
            return false;
        }
        winsockReady = true;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    addrinfo hints{};
    addrinfo* result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    string portText = to_string(port);
    if (getaddrinfo(host.c_str(), portText.c_str(), &hints, &result) != 0)
    {
        SetError("Could not resolve server address.");
        return false;
    }

    Socket socketHandle = INVALID_SOCK;
    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
    {
        socketHandle = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (socketHandle == INVALID_SOCK) continue;

        if (connect(socketHandle, ptr->ai_addr, (int)ptr->ai_addrlen) == 0)
            break;

        CLOSE_SOCKET(socketHandle);
        socketHandle = INVALID_SOCK;
    }

    freeaddrinfo(result);

    if (socketHandle == INVALID_SOCK)
    {
        SetError("Could not connect to server.");
        return false;
    }

    gSocket = socketHandle;

    if (!SendAll(gSocket, username + "\n"))
    {
        CLOSE_SOCKET(gSocket);
        gSocket = INVALID_SOCK;
        SetError("Connected, but failed to send username.");
        return false;
    }

    {
        lock_guard<mutex> lock(gQueueMutex);
        while (!gMessages.empty()) gMessages.pop();
    }

    gRunning = true;
    gConnected = true;
    gReceiver = thread(ReceiverLoop);
    return true;
}

void NetDisconnect()
{
    gRunning = false;
    gConnected = false;

    if (gSocket != INVALID_SOCK)
    {
        shutdown(gSocket, SHUT_BOTH);
        CLOSE_SOCKET(gSocket);
        gSocket = INVALID_SOCK;
    }

    if (gReceiver.joinable()) gReceiver.join();
}

bool NetIsConnected()
{
    return gConnected;
}

bool NetSendLine(const string& line)
{
    if (!gConnected || gSocket == INVALID_SOCK) return false;

    if (!SendAll(gSocket, line + "\n"))
    {
        SetError("Failed to send message.");
        return false;
    }
    return true;
}

vector<NetMessage> NetPollMessages()
{
    vector<NetMessage> result;
    lock_guard<mutex> lock(gQueueMutex);

    while (!gMessages.empty())
    {
        result.push_back(gMessages.front());
        gMessages.pop();
    }

    return result;
}
