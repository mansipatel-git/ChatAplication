#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <unistd.h>
#include <netinet/in.h>

#define PORT 5555

using namespace std;

// Maps username to their socket descriptor
unordered_map<string, int> user_to_socket;

// Maps socket descriptor to username
unordered_map<int, string> socket_to_user;

// Maps room names to the set of socket descriptors (clients) in that room
unordered_map<string, unordered_set<int>> rooms;

// Tracks messages by unique ID and stores message content, sender, and room
unordered_map<int, string> message_id_to_content;
unordered_map<int, string> message_id_to_sender;
unordered_map<int, string> message_id_to_room;

// Global message counter and associated mutex
int message_counter = 1;
mutex rooms_mutex;   // Protects access to room/user data
mutex counter_mutex; // Protects access to message_counter

// Logs messages to a file corresponding to the room
void log_message(const string& room, const string& message) {
    time_t now = time(nullptr);
    tm* ltm = localtime(&now);

    ofstream log_file(room + ".log", ios::app);
    if (log_file.is_open()) {
        log_file << "[" << 1900 + ltm->tm_year << "-"
                 << 1 + ltm->tm_mon << "-"
                 << ltm->tm_mday << " "
                 << ltm->tm_hour << ":"
                 << ltm->tm_min << ":"
                 << ltm->tm_sec << "] "
                 << message << endl;
    }
}

// Sends a message to all users in the specified room
void broadcast_message(const string& room, const string& message) {
    lock_guard<mutex> lock(rooms_mutex);
    for (int client : rooms[room]) {
        send(client, message.c_str(), message.length(), 0);
    }
}

// Sends a private message to a specific user
void send_private_message(const string& sender, const string& recipient, const string& message) {
    lock_guard<mutex> lock(rooms_mutex);
    if (user_to_socket.count(recipient)) {
        int target_fd = user_to_socket[recipient];
        string full_msg = "(Private) " + sender + ": " + message;
        send(target_fd, full_msg.c_str(), full_msg.length(), 0);
    } else {
        string error_msg = "User " + recipient + " not found.\n";
        send(user_to_socket[sender], error_msg.c_str(), error_msg.length(), 0);
    }
}

// Handles communication for an individual client
void handle_client(int client_socket) {
    char buffer[1024];
    string username, room;

    // --- Initial handshake: receive username ---
    ssize_t name_len = recv(client_socket, buffer, sizeof(buffer), 0);
    if (name_len <= 0) {
        close(client_socket);
        return;
    }
    username = string(buffer, name_len);

    // --- Receive room name ---
    ssize_t room_len = recv(client_socket, buffer, sizeof(buffer), 0);
    if (room_len <= 0) {
        close(client_socket);
        return;
    }
    room = string(buffer, room_len);

    // --- Add client to room and maps ---
    {
        lock_guard<mutex> lock(rooms_mutex);
        rooms[room].insert(client_socket);
        user_to_socket[username] = client_socket;
        socket_to_user[client_socket] = username;
    }

    // Notify others in the room
    log_message(room, username + " has joined the room.");
    broadcast_message(room, username + " has joined the room.");

    // --- Main message handling loop ---
    while (true) {
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            // If client disconnects, clean up
            lock_guard<mutex> lock(rooms_mutex);
            rooms[room].erase(client_socket);
            user_to_socket.erase(username);
            socket_to_user.erase(client_socket);
            close(client_socket);

            broadcast_message(room, username + " has left the room.");
            log_message(room, username + " has left the room.");
            break;
        }

        // Extract full message
        string msg(buffer, bytes_received);

        // --- Handle commands ---
        if (msg.rfind("/msg ", 0) == 0) {
            // Private message: format => /msg recipient message
            size_t space1 = msg.find(' ', 5);
            if (space1 != string::npos) {
                string target_user = msg.substr(5, space1 - 5);
                string private_msg = msg.substr(space1 + 1);
                send_private_message(username, target_user, private_msg);
            }

        } else if (msg.rfind("/edit ", 0) == 0) {
            // Edit message: format => /edit message_id new_message
            size_t space1 = msg.find(' ', 6);
            if (space1 != string::npos) {
                int message_id = stoi(msg.substr(6, space1 - 6));
                string new_message = msg.substr(space1 + 1);
                if (message_id_to_sender.count(message_id) && message_id_to_sender[message_id] == username) {
                    message_id_to_content[message_id] = new_message;
                    string edited_msg = username + " edited their message: " + new_message;
                    broadcast_message(room, edited_msg);
                    log_message(room, "Edited Message: " + edited_msg);
                } else {
                    string error_msg = "Invalid message ID or you're not the sender.\n";
                    send(client_socket, error_msg.c_str(), error_msg.length(), 0);
                }
            }

        } else if (msg.rfind("/delete ", 0) == 0) {
            // Delete message: format => /delete message_id
            int message_id = stoi(msg.substr(8));
            if (message_id_to_sender.count(message_id) && message_id_to_sender[message_id] == username) {
                string deleted_msg = username + " deleted their message.";
                broadcast_message(room, deleted_msg);
                log_message(room, "Message deleted: " + deleted_msg);

                // Remove message from storage
                message_id_to_content.erase(message_id);
                message_id_to_sender.erase(message_id);
                message_id_to_room.erase(message_id);
            } else {
                string error_msg = "Invalid message ID or you're not the sender.\n";
                send(client_socket, error_msg.c_str(), error_msg.length(), 0);
            }

        } else {
            // --- Regular message handling ---
            int message_id;
            {
                lock_guard<mutex> lock(counter_mutex);
                message_id = message_counter++;
            }

            // Store message
            message_id_to_content[message_id] = msg;
            message_id_to_sender[message_id] = username;
            message_id_to_room[message_id] = room;

            // Format and broadcast message
            string full_msg = username + ": " + msg;
            log_message(room, "Message: " + full_msg);
            broadcast_message(room, full_msg);
        }
    }
}

// Entry point of the server
int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    // --- Create TCP socket ---
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        cerr << "Failed to create socket\n";
        return 1;
    }

    // Allow socket reuse (helps during quick restarts)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "setsockopt failed\n";
        return 1;
    }

    // --- Bind socket to IP/PORT ---
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;      // Accept connections from any IP
    address.sin_port = htons(PORT);            // Host to network byte order

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        cerr << "Bind failed\n";
        return 1;
    }

    // --- Start listening for connections ---
    if (listen(server_fd, 10) < 0) {
        cerr << "Listen failed\n";
        return 1;
    }

    cout << "Server listening on port " << PORT << "...\n";

    // --- Accept loop ---
    while (true) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket < 0) {
            cerr << "Accept failed\n";
            continue;
        }

        // Handle each client in a new thread
        thread(handle_client, new_socket).detach();
    }

    return 0;
}

