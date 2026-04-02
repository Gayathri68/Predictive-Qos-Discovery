# Computer Networks Project Report: Predictive QoS Registry & Load Balancer

---

## 1. Introduction

With the exponential growth of distributed systems and microservice architectures, balancing application load efficiently is paramount. Traditional round-robin or randomized load balancers suffer from a lack of environmental awareness; they assign tasks blindly. 

This project solves this problem by introducing a **Predictive QoS (Quality of Service) Registry**. Designed from scratch using low-level C++ Windows Sockets (Winsock2), this centralized smart-edge load balancer actively monitors the real-time resource utilization (CPU and RAM) of individual service nodes across the network. By dynamically calculating a "QoS Score" for every connected node, the registry can intuitively route incoming client application requests to the healthiest and most capable server.

## 2. Objectives

- **Protocol Application**: To demonstrate the practical differences and synergies between stream-oriented (TCP) and datagram-oriented (UDP) communication.
- **Dynamic Load Balancing**: To implement a custom load-balancing algorithm capable of making routing decisions based on live system metrics.
- **Failover & Fault Tolerance**: To guarantee uninterrupted service by automatically evicting unresponsive or failed server nodes within a predefined timeout.
- **Multithreading**: To implement concurrent socket handling using native Windows APIs (`CreateThread`, `CRITICAL_SECTION`) to ensure the registry operates without blocking network I/O.
- **Data Visualization**: To serve real-time statistics to a web dashboard via a custom HTTP/JSON pipeline written entirely in C++.

## 3. System Architecture

The ecosystem relies on an asynchronous, multi-threaded Primary/Replica architectural pattern comprising three core components:

### 3.1 The C++ Registry Server (Central Load Balancer)
The absolute core of the project. It launches three asynchronous worker threads:
1. **The UDP Ingestion Thread**: Bound to port `9001`. Extremely fast and lightweight. It continuously listens for incoming datagrams containing heartbeat structures from all active nodes.
2. **The TCP Client Router Thread**: Bound to port `9000`. Exists to handle standard queries from connecting clients. It locks the active node table, executes the load-balancing algorithm, and returns the details of the most mathematically optimal node over the reliable TCP channel. 
3. **The HTTP Dashboard Thread**: Bound to port `9002`. Mimics a lightweight RESTful API by parsing basic HTTP `GET` requests and serializing the internal memory map of nodes into a JSON payload, serving it to the graphical dashboard over HTTP.

### 3.2 The Service Nodes (Worker Threads)
These are the applications doing the actual heavy lifting. They host a TCP server on a dynamic port to satisfy client requests. In the background, a worker thread generates synthetic CPU and RAM load percentages (ranging from 10% to 90%) and wraps them into a `Heartbeat` struct to be blasted over UDP to the Registry Server every 2 seconds.

### 3.3 The Smart Client
The client logic operates in two sequential phases:
- **Phase 1**: Opens a TCP connection to the Registry Server to request a Node assignment. 
- **Phase 2**: Reads the IP and Port returned by the Registry, terminates the registry connection, and initiates a direct TCP connection with the assigned node to execute the application level tasks.

## 4. Network Protocol Decisions

### Why UDP for Heartbeats?
Service nodes transmit their workloads continuously (every 2 seconds). Using TCP for this would mandate a costly three-way handshake and acknowledgment overhead merely to transmit a few bytes of telemetry. By utilizing **UDP (User Datagram Protocol)**, the nodes can rapidly "fire and forget" their telemetry statistics. Minor packet loss is acceptable because the next heartbeat will arrive seconds later.

### Why TCP for Client Queries?
When a client requests a service endpoint, packet loss could yield a broken application flow. To guarantee reliable routing, the primary client queries are conducted over **TCP (Transmission Control Protocol)**.

## 5. Load Balancing Algorithm & Logic

At any given interval, the Registry contains an isolated map data structure holding all active nodes. When a client requests a node, the registry calculates a "QoS Matrix Score" for every connected machine based on predefined heuristics. 

**QoS Score Formula**:
`Score = (0.6 * Normalized CPU) + (0.4 * Normalized RAM)`

The system iterates across the entire map, and the node boasting the absolute lowest calculated score (meaning it has the fewest bottlenecks) is dynamically returned to the client.

**Fault Tolerance (Stale Node Eviction)**:
Whenever the registry locks the table to respond to a client, it actively verifies the `LAST_SEEN` timestamp of every node. If a node has neglected to send a UDP heartbeat for more than **5 seconds**, the registry deduces that the node has crashed or experienced a localized network failure, and immediately deletes it from memory, protecting future clients from connecting to a dead server.

## 6. Real-Time Analytics Dashboard

To visualize network traffic, the system employs a modern, reactive graphical dashboard constructed with pure HTML, CSS (featuring Glassmorphism UI patterns), and JavaScript. 

The dashboard actively polls the localized HTTP server embedded within the C++ Registry (`http://localhost:9002/api/state`) at a rate of 1 Hertz. The JavaScript asynchronous `fetch()` API ingests the network metrics and translates the load parameters into responsive CSS progress bars, demonstrating the load balancer dynamically adapting to synthetic strain in real-time.

## 7. Challenges & Resolutions

- **Concurrency & Race Conditions**: Handling rapid UDP telemetry metrics simultaneously with TCP client queries required meticulous memory locking. Handled effectively using Windows `CRITICAL_SECTION` arrays, ensuring the registry mapping was never read during a mutating write.
- **Cross-Component Communication**: Providing dashboard analytics from a purely low-level C++ network application necessitated reverse-engineering the HTTP application layer protocol. This was achieved by constructing raw HTTP packet responses incorporating the `Content-Type: application/json` headers directly in C++.

## 8. Conclusion and Future Scope

This project successfully proves the efficacy of dynamic routing protocols heavily rooted in low-level socket programming. Implementing predicting algorithms significantly bolsters network response latency dynamically responding to strain.

**Future Deployments**:
While synthetic loads were necessary for a localized simulation, future iterations could easily replace the RNG mechanisms with standard Windows Performance Counters (`GetSystemTimes`, `GlobalMemoryStatusEx`) to monitor actual host machine vitals across a real Local Area Network (LAN).
