#pragma once

#include <string>
#include <vector>

struct NetMessage
{
    std::string type;
    std::string data;
};

bool NetConnect(const std::string& host, int port, const std::string& username);
void NetDisconnect();
bool NetIsConnected();
bool NetSendLine(const std::string& line);
std::vector<NetMessage> NetPollMessages();
std::string NetLastError();
