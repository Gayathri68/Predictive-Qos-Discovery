#define _WIN32_WINNT 0x0601

#include <iostream>
#include <vector>
#include <string>

#include <thread>
#include <mutex>
#include <chrono>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <random>
#include "common.h"
// Global variable for the node's listening port
int node_port;

// UDP Thread: Send heartbeats with synthetic load data
DWORD WINAPI UdpHeartbeatSender(LPVOID lpParam) {
    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create UDP socket.\n";
        return 1;
    }

    sockaddr_in registryAddr;
    registryAddr.sin_family = AF_INET;
    registryAddr.sin_port = htons(REGISTRY_UDP_PORT);
    registryAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Setup random number generation for synthetic load metrics
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> load_dist(10.0, 90.0);

    while (true) {
        NodeHeartbeat hb;
        strncpy(hb.ip, "127.0.0.1", sizeof(hb.ip) - 1);
        hb.ip[sizeof(hb.ip) - 1] = '\0';
        hb.service_port = node_port;
        
        // Generate random load
        hb.cpu_load = load_dist(gen);
        hb.ram_load = load_dist(gen);

        if (sendto(udpSocket, (char*)&hb, sizeof(hb), 0, (sockaddr*)&registryAddr, sizeof(registryAddr)) == SOCKET_ERROR) {
            std::cerr << "[Node-" << node_port << "] Error sending heartbeat.\n";
        } else {
            std::cout << "[Node-" << node_port << "] Sent heartbeat (CPU: " 
                      << hb.cpu_load << "%, RAM: " << hb.ram_load << "%)\n";
        }

        // Send a heartbeat every 2 seconds
        Sleep(2000);
    }
    closesocket(udpSocket);
    return 0;
}

// TCP Thread: Serve application requests from the client
DWORD WINAPI TcpServiceProvider(LPVOID lpParam) {
    SOCKET tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcpSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create TCP socket.\n";
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(node_port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcpSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind TCP socket on port " << node_port << ". Maybe it's in use?\n";
        closesocket(tcpSocket);
        return 1;
    }

    listen(tcpSocket, SOMAXCONN);
    std::cout << "[Node-" << node_port << "] Listening for clients on TCP port " << node_port << "\n";

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(tcpSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket != INVALID_SOCKET) {
            std::cout << "[Node-" << node_port << "] Accepted client connection!\n";
            char buffer[256];
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                std::cout << "[Node-" << node_port << "] Client sent: " << buffer << "\n";
                
                // Process request and send a reply indicating which node answered
                std::string reply = "Service successfully provided by Node on port " + std::to_string(node_port) + "!";
                send(clientSocket, reply.c_str(), reply.size(), 0);
            }
            closesocket(clientSocket);
        }
    }
    closesocket(tcpSocket);
    return 0;
}

int main(int argc, char* argv[]) {
    // Port must be supplied as an argument so we can run multiple nodes locally
    if (argc < 2) {
        std::cout << "Usage: service_node.exe <ServicePort>\n";
        std::cout << "Example: service_node.exe 8001\n";
        return 1;
    }

    node_port = std::stoi(argv[1]);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    std::cout << "=== Starting Service Node on port " << node_port << " ===\n";

    // Launch worker threads
    HANDLE udpThread = CreateThread(NULL, 0, UdpHeartbeatSender, NULL, 0, NULL);
    HANDLE tcpThread = CreateThread(NULL, 0, TcpServiceProvider, NULL, 0, NULL);

    WaitForSingleObject(udpThread, INFINITE);
    WaitForSingleObject(tcpThread, INFINITE);

    WSACleanup();
    return 0;
}
