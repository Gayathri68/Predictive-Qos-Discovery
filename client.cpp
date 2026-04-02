#define _WIN32_WINNT 0x0601

#include <iostream>
#include <vector>
#include <string>

#include <thread>
#include <mutex>
#include <chrono>

#include <winsock2.h>
#include <ws2tcpip.h>
#include "common.h"

void RunClient() {
    // Phase 1: Contact Registry to get the optimal node
    SOCKET registrySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (registrySocket == INVALID_SOCKET) {
        std::cerr << "Failed to create Registry TCP socket.\n";
        return;
    }

    sockaddr_in registryAddr;
    registryAddr.sin_family = AF_INET;
    registryAddr.sin_port = htons(REGISTRY_TCP_PORT);
    registryAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(registrySocket, (sockaddr*)&registryAddr, sizeof(registryAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to Discovery Registry on port " << REGISTRY_TCP_PORT << ".\n";
        closesocket(registrySocket);
        return;
    }

    std::cout << "[Client] Connected to Registry. Requesting best node...\n";
    const char* requestMsg = "GET_NODE";
    send(registrySocket, requestMsg, strlen(requestMsg), 0);

    BestNodeResponse response;
    int bytesReceived = recv(registrySocket, (char*)&response, sizeof(response), 0);
    closesocket(registrySocket);

    // Validate the registry response
    if (bytesReceived != sizeof(BestNodeResponse) || !response.success) {
        std::cerr << "[Client] Registry failed to find an active node.\n";
        return;
    }

    std::cout << "[Client] Registry assigned optimal Node at " 
              << response.ip << ":" << response.port << "\n";

    // Phase 2: Connect to the best service node and execute the actual application request
    SOCKET nodeSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nodeSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create Node TCP socket.\n";
        return;
    }

    sockaddr_in nodeAddr;
    nodeAddr.sin_family = AF_INET;
    nodeAddr.sin_port = htons(response.port);
    nodeAddr.sin_addr.s_addr = inet_addr(response.ip);

    if (connect(nodeSocket, (sockaddr*)&nodeAddr, sizeof(nodeAddr)) == SOCKET_ERROR) {
        std::cerr << "[Client] Failed to connect to the assigned mode.\n";
        closesocket(nodeSocket);
        return;
    }

    std::cout << "[Client] Connected to target Node! Sending action request...\n";
    const char* actionMsg = "DO_WORK";
    send(nodeSocket, actionMsg, strlen(actionMsg), 0);

    char replyBuffer[256];
    int replyBytes = recv(nodeSocket, replyBuffer, sizeof(replyBuffer) - 1, 0);
    if (replyBytes > 0) {
        replyBuffer[replyBytes] = '\0';
        std::cout << "[Client] Received response from Node: " << replyBuffer << "\n";
    }

    closesocket(nodeSocket);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    std::cout << "=== Starting Smart Client ===\n";
    RunClient();

    WSACleanup();
    
    std::cout << "Press Enter to exit.\n";
    std::cin.get();
    return 0;
}
