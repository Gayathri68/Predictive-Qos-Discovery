#define _WIN32_WINNT 0x0601

#include <iostream>
#include <vector>
#include <string>

#include <thread>
#include <mutex>
#include <chrono>

#include <winsock2.h>
#include <ws2tcpip.h>


// Ensure Winsock is linked (works in MSVC, may need -lws2_32 in g++)
#pragma comment(lib, "ws2_32.lib")

// Port configurations
constexpr int REGISTRY_UDP_PORT = 9001; // Service nodes -> Registry (Heartbeats)
constexpr int REGISTRY_TCP_PORT = 9000; // Clients -> Registry (Queries)

// Heartbeat payload sent via UDP from the Service Node to the Registry
struct NodeHeartbeat {
    char ip[16];        // IP address of the node (e.g., "127.0.0.1")
    int service_port;   // TCP port the node is listening on for clients
    double cpu_load;    // Random/Simulated CPU load percentage (0.0 to 100.0)
    double ram_load;    // Random/Simulated RAM load percentage (0.0 to 100.0)
};

// Response payload sent via TCP from the Registry to the Client
struct BestNodeResponse {
    bool success;       // true if we found a node, false if all nodes are offline
    char ip[16];        // IP address of the best node
    int port;           // TCP port of the best node
};
