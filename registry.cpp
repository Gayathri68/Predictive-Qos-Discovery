#define _WIN32_WINNT 0x0601

#include <iostream>
#include <unordered_map>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Ports
const int REGISTRY_TCP_PORT = 9000;
const int REGISTRY_UDP_PORT = 9001;
const int NODE_TIMEOUT = 5;

// Node structure
struct NodeInfo {
    string ip;
    int port;
    double cpu;
    double ram;
    chrono::time_point<chrono::system_clock> last_seen;
};

unordered_map<string, NodeInfo> registry;
CRITICAL_SECTION registry_lock;

// Heartbeat structure
struct Heartbeat {
    char ip[16];
    int port;
    double cpu;
    double ram;
};

// Remove stale nodes
void removeStaleNodes() {

    auto now = chrono::system_clock::now();

    for (auto it = registry.begin(); it != registry.end(); ) {

        auto diff = chrono::duration_cast<chrono::seconds>(
            now - it->second.last_seen).count();

        if (diff > NODE_TIMEOUT) {
            cout << "[Registry] Removing stale node: "
                 << it->first << endl;
            it = registry.erase(it);
        }
        else {
            ++it;
        }
    }
}

// UDP Thread
DWORD WINAPI udpListener(LPVOID lpParam) {

    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(REGISTRY_UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(udpSocket, (sockaddr*)&addr, sizeof(addr));

    cout << "[Registry] Listening for heartbeats on UDP "
         << REGISTRY_UDP_PORT << endl;

    while (true) {

        Heartbeat hb{};
        sockaddr_in sender{};
        int senderSize = sizeof(sender);

        int bytes = recvfrom(udpSocket,
                             (char*)&hb,
                             sizeof(hb),
                             0,
                             (sockaddr*)&sender,
                             &senderSize);

        if (bytes == sizeof(Heartbeat)) {

            EnterCriticalSection(&registry_lock);

            string key = string(hb.ip) + ":" +
                         to_string(hb.port);

            registry[key] = {
                hb.ip,
                hb.port,
                hb.cpu,
                hb.ram,
                chrono::system_clock::now()
            };

            cout << "[Registry] Updated node "
                 << key
                 << " CPU:" << hb.cpu
                 << " RAM:" << hb.ram
                 << endl;

            LeaveCriticalSection(&registry_lock);
        }
    }

    return 0;
}

// TCP Server (Main Thread)
void tcpServer() {

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(REGISTRY_TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&addr, sizeof(addr));
    listen(serverSocket, SOMAXCONN);

    cout << "[Registry] TCP server running on port "
         << REGISTRY_TCP_PORT << endl;

    while (true) {

        SOCKET clientSocket =
            accept(serverSocket, NULL, NULL);

        char buffer[128];
        int bytes = recv(clientSocket,
                         buffer,
                         sizeof(buffer) - 1,
                         0);

        if (bytes > 0) {

            EnterCriticalSection(&registry_lock);

            removeStaleNodes();

            string bestIP = "";
            int bestPort = -1;
            double bestScore = 1e9;

            for (auto& pair : registry) {

               double cpu_norm = pair.second.cpu / 100.0;
               double ram_norm = pair.second.ram / 100.0;

               double score = 0.6 * cpu_norm + 0.4 * ram_norm;

                if (score < bestScore) {
                    bestScore = score;
                    bestIP = pair.second.ip;
                    bestPort = pair.second.port;
                }
            }

            LeaveCriticalSection(&registry_lock);

            if (bestPort != -1) {
                struct {
                    bool success;
                    char ip[16];
                    int port;
                } response;
                response.success = true;
                strncpy(response.ip, bestIP.c_str(), sizeof(response.ip) - 1);
                response.ip[sizeof(response.ip) - 1] = '\0';
                response.port = bestPort;

                send(clientSocket,
                     (char*)&response,
                     sizeof(response),
                     0);
            }
            else {
                struct {
                    bool success;
                    char ip[16];
                    int port;
                } response;
                response.success = false;
                
                send(clientSocket,
                     (char*)&response,
                     sizeof(response),
                     0);
            }
        }

        closesocket(clientSocket);
    }
}

// HTTP Thread: Serve dashboard JSON on port 9002
DWORD WINAPI HttpDashboardApi(LPVOID lpParam) {
    SOCKET httpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (httpSocket == INVALID_SOCKET) {
        std::cerr << "HTTP socket creation failed.\n";
        return 1; 
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9002);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(httpSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "HTTP bind failed.\n";
        closesocket(httpSocket);
        return 1;
    }

    listen(httpSocket, SOMAXCONN);
    std::cout << "[Registry] HTTP Dashboard API started on port 9002.\n";

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(httpSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) continue;

        char buffer[1024];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            
            // Only respond to GET requests
            if (strncmp(buffer, "GET", 3) == 0) {
                EnterCriticalSection(&registry_lock);
                std::string json = "[";
                bool first = true;
                for (const auto& pair : registry) {
                    if (!first) json += ",";
                    json += "{";
                    json += "\"id\":\"" + pair.first + "\",";
                    json += "\"ip\":\"" + pair.second.ip + "\",";
                    json += "\"port\":" + std::to_string(pair.second.port) + ",";
                    json += "\"cpu\":" + std::to_string(pair.second.cpu) + ",";
                    json += "\"ram\":" + std::to_string(pair.second.ram) + ",";
                    double score = (0.6 * pair.second.cpu) + (0.4 * pair.second.ram);
                    json += "\"score\":" + std::to_string(score);
                    json += "}";
                    first = false;
                }
                json += "]";
                LeaveCriticalSection(&registry_lock);

                std::string response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Connection: close\r\n\r\n" + json;

                send(clientSocket, response.c_str(), response.size(), 0);
            }
        }
        closesocket(clientSocket);
    }
    closesocket(httpSocket);
    return 0;
}

void dashboardServer() {

    SOCKET httpSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(httpSocket, (sockaddr*)&addr, sizeof(addr));
    listen(httpSocket, SOMAXCONN);

    cout << "[Dashboard] Running at http://localhost:8080" << endl;

    while (true) {

        SOCKET client = accept(httpSocket, NULL, NULL);

        EnterCriticalSection(&registry_lock);

        string html =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n\r\n";

        html += "<html><head><title>QoS Registry Dashboard</title>";
        html += "<style>";
        html += "body{font-family:Arial;background:#1e1e2f;color:white;}";
        html += "table{border-collapse:collapse;width:80%;margin:auto;}";
        html += "th,td{border:1px solid white;padding:10px;text-align:center;}";
        html += "th{background:#333;}";
        html += ".good{color:lightgreen;}";
        html += ".bad{color:red;}";
        html += "</style></head><body>";

        html += "<h1 style='text-align:center;'>QoS Registry Dashboard</h1>";
        html += "<h3 style='text-align:center;'>Active Nodes: ";
        html += to_string(registry.size()) + "</h3>";

        html += "<table>";
        html += "<tr><th>Node</th><th>CPU (%)</th><th>RAM (%)</th><th>Status</th></tr>";

        auto now = chrono::system_clock::now();

        for (auto& pair : registry) {

            auto diff = chrono::duration_cast<chrono::seconds>(
                now - pair.second.last_seen).count();

            string status = (diff <= NODE_TIMEOUT) ?
                "<span class='good'>ONLINE</span>" :
                "<span class='bad'>OFFLINE</span>";

            html += "<tr>";
            html += "<td>" + pair.first + "</td>";
            html += "<td>" + to_string(pair.second.cpu) + "</td>";
            html += "<td>" + to_string(pair.second.ram) + "</td>";
            html += "<td>" + status + "</td>";
            html += "</tr>";
        }

        html += "</table>";
        html += "</body></html>";

        LeaveCriticalSection(&registry_lock);

        send(client, html.c_str(), html.size(), 0);
        closesocket(client);
    }
}

int main() {

    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    InitializeCriticalSection(&registry_lock);

    cout << "=== Simple QoS Registry Starting ===" << endl;

    HANDLE udpThread = CreateThread(
        NULL,
        0,
        udpListener,
        NULL,
        0,
        NULL
    );
    HANDLE dashboardThread = CreateThread(
    NULL,
    0,
    (LPTHREAD_START_ROUTINE)dashboardServer,
    NULL,
    0,
    NULL
);
    HANDLE httpApiThread = CreateThread(
    NULL,
    0,
    HttpDashboardApi,
    NULL,
    0,
    NULL
);

    tcpServer();

    DeleteCriticalSection(&registry_lock);
    WSACleanup();

    return 0;
}