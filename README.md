# Distributed File System

## About
🖥️ A distributed file system where specialized servers manage different file types across multiple storage nodes. Supports automatic file routing, full file operations (upload, download, delete, list), dynamic path resolution, and parallel processing. The client interface ensures seamless and efficient management of files over the distributed architecture.

## Features
- 🚀 **Automatic File Routing:** Files are routed to specialized servers based on type.
- 📂 **Comprehensive File Operations:** Upload, download, delete, and list files.
- 🗺️ **Path Resolution:** Dynamic and intelligent path management.
- 🧵 **Parallel Processing:** Boosts performance by handling multiple operations simultaneously.
- 🖥️ **Client Interface:** Easy-to-use client application to interact with the system.

## System Architecture
- **Specialized Servers:** Each server handles specific file types (e.g., text, images, videos).
- **Client Application:** Connects to multiple servers, managing distributed storage as a unified system.
- **Communication Protocols:** Efficient handling of requests over the network.

## Requirements
- GCC compiler (for C/C++ builds)
- POSIX-compliant OS (Linux/Unix preferred)
- Basic networking libraries (socket programming)

## How to Compile
```bash
gcc -o server server.c
gcc -o client client.c
```

## How to Run
1. **Start Specialized Servers:**
    ```bash
    ./server
    ```
2. **Run the Client Interface:**
    ```bash
    ./client
    ```

## Example Operations
- **Upload a file:**
  - Client automatically selects the server.
- **Download a file:**
  - Client fetches from the correct server.
- **List all files:**
  - View distributed file catalog.
- **Delete a file:**
  - Remove specific files from nodes.

## Notes
- Ensure all servers are running before starting the client.
- Optimize network settings for best performance.

## Contribution
Contributions are welcome! Please fork the repository and submit a pull request.

## License
Open-source project under the MIT License.

---

*Built to simplify distributed file management with speed and intelligence.*

