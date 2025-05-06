
# 🗨️ Real-Time Chat Application (C++)

A simple, extensible real-time chat system built using **C++**, **POSIX sockets**, and **multithreading**. Supports chat rooms, private messaging, message editing/deleting, and persistent logging.

## 📌 Features

- ✅ Multi-room support  
- ✅ Broadcast and private messaging (`/msg`)  
- ✅ Command handling (`/edit`, `/delete`)  
- ✅ Message logging to `.log` files  
- ✅ Concurrent message receiving via threads  
- ✅ Thread-safe using `std::mutex`

## 🧱 Architecture

### Client
- Connects to server (`127.0.0.1:5555`)
- Prompts for username and room
- Sends messages to server
- Receives messages using a dedicated thread

### Server
- Accepts multiple clients using threads
- Maintains:
  - User-to-socket mappings
  - Room membership tracking
  - Message storage for editing/deleting
- Logs all messages to timestamped `.log` files

## 🧪 Commands

| Command                  | Description                                |
|--------------------------|--------------------------------------------|
| `/msg <user> <message>`  | Send a private message                     |
| `/edit <id> <new text>`  | Edit your own message by ID               |
| `/delete <id>`           | Delete your own message by ID             |

## 🛠️ Dependencies

- C++11 or later
- POSIX-compatible system (Linux, macOS, WSL)

## 🔧 How to Run

### 1. Compile Server and Client

```bash
g++ -std=c++11 server.cpp -o server -pthread
g++ -std=c++11 client.cpp -o client -pthread
