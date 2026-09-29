#pragma once

#include <string>
#include <vector>

struct NetMessage
{
    std::string type;
    std::string data;
};

bool NetConnect(const std::string& host, int port, const std::string& username,
    const std::string& password, bool createAccount);
void NetDisconnect();
bool NetIsConnected();
bool NetIsConnecting();
bool NetSendLine(const std::string& line);
std::vector<NetMessage> NetPollMessages();
std::string NetLastError();
