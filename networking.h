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
bool NetChangeUsername(const std::string& newUsername, const std::string& currentPassword);
bool NetChangePassword(const std::string& currentPassword, const std::string& newPassword);
bool NetDeleteAccount(const std::string& currentPassword);
std::vector<NetMessage> NetPollMessages();
std::string NetLastError();
