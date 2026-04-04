# Predictive QoS Registry & Load Balancer

![C++](https://img.shields.io/badge/Language-C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Windows](https://img.shields.io/badge/Platform-Windows%20(Winsock2)-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![HTML/CSS/JS](https://img.shields.io/badge/Frontend-HTML%20%7C%20CSS%20%7C%20JS-E34F26?style=for-the-badge&logo=html5&logoColor=white)

A high-performance, multithreaded service discovery system and load balancer written entirely from scratch in C++ using low-level Windows Sockets (Winsock2). This project actively monitors the real-time resource utilization (CPU and RAM) of individual service nodes and routes incoming client requests to the healthiest server dynamically.

![Dashboard Demo](demo.webp)

## 🌟 Key Features

*   **Dynamic Load Balancing:** Calculates a real-time QoS Score `(0.6 * CPU) + (0.4 * RAM)` for active nodes to mathematically select the best server for an incoming client.
*   **Dual-Protocol Architecture:** Uses extremely fast **UDP** for continuous worker node heartbeats and reliable **TCP** for mission-critical client routing queries.
*   **Built-in Fault Tolerance:** Implements an asynchronous eviction mechanism that automatically drops unresponsive nodes if a UDP heartbeat hasn't been received in 5 seconds.
*   **High Concurrency:** Utilizes native Windows multi-threading (`CreateThread`, `CRITICAL_SECTION`) to ensure the registry operates without blocking network I/O.
*   **Custom HTTP/JSON Server:** The C++ registry hosts a custom-built lightweight HTTP server to serve real-time statistical JSON payloads.
*   **Real-Time Analytics Dashboard:** A glassmorphic web dashboard (HTML/CSS/JS) that polls the C++ registry to visually render the network's health and load balancing actions dynamically.

## 🏗️ System Architecture

The ecosystem relies on an asynchronous, multi-threaded Primary/Replica architectural pattern comprising three core components:

1.  **The Registry Server (`registry.cpp`)**
    *   **Port 9000 (TCP):** Handles standard queries from clients, executing the load-balancing logic.
    *   **Port 9001 (UDP):** Ingests lightweight telemetry heartbeats from worker nodes.
    *   **Port 9002 (HTTP/TCP):** Serves the JSON state API (`/api/state`) for the web dashboard.
2.  **The Service Nodes (`service_node.cpp`)**
    *   Host their own TCP servers to handle actual application tasks.
    *   Actively blast telemetry data (CPU/RAM metrics) over UDP to the Registry every 2 seconds.
3.  **The Smart Client (`client.cpp`)**
    *   Queries the Registry via TCP to receive the most optimal Service Node IP/Port, then connects to that Node directly.

## 🚀 How to Run

### Prerequisites
*   Windows OS (Relies on `winsock2.h` and `<windows.h>`)
*   A C++ Compiler like GCC (MinGW-w64) or MSVC.

### 1. Compilation
Compile the distinct components into executables. For example, using `g++`:

```bash
g++ registry.cpp -o registry.exe -lws2_32
g++ service_node.cpp -o service_node.exe -lws2_32
g++ client.cpp -o client.exe -lws2_32
```

### 2. Execution Flow
Starting the components in the correct order is important to visualize the network.

1.  **Start the Registry:** Run `registry.exe`. This starts the UDP listener, TCP router, and HTTP server.
2.  **Start the Dashboard:** Open `dashboard/index.html` in any modern web browser to view the real-time visualizer.
3.  **Start Nodes:** Run multiple instances of `service_node.exe`. You will see them appear dynamically on the dashboard.
4.  **Simulate Clients:** Run `client.exe`. The registry will evaluate the node metrics and confidently route the client to the node under the least stress.

## 🧠 Why Build This? (Academic Goals)
Built as an academic Computer Networks project mapped towards demonstrating:
*   The practical differences and synergies between stream-oriented (TCP) and datagram-oriented (UDP) communication paradigms.
*   Deep understanding of non-blocking I/O operations, custom protocol design, and thread-safety over raw sockets.
*   Abstracting low-level memory arrays into high-level visualizations via custom HTTP parsing.
