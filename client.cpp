#include <iostream>      // For standard I/O
#include <thread>        // For handling concurrent message receiving
#include <unistd.h>      // For close(), sleep()
#include <netinet/in.h>  // For sockaddr_in
#include <arpa/inet.h>   // For inet_pton()

#define PORT 5555        // Port number to connect to the server

using namespace std;

// Function to receive messages from the server
void receive_messages(int sock) {
    char buffer[1024];  // Buffer to hold incoming messages
    while (true) {
        // Wait to receive data from server
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_received > 0) {
            // If message received, print it and re-display prompt
            cout << "\n" << string(buffer, bytes_received) << "\n> ";
            cout.flush();  // Make sure prompt shows up immediately
        } else if (bytes_received == 0) {
            // Server disconnected cleanly
            cout << "\nDisconnected from server.\n";
            break;
        }
        // Note: No handling for recv() < 0 (error case); could be added
    }
}

int main() {
    // Create a TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cerr << "Failed to create socket\n";
        return 1;
    }

    // Define server address structure
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;               // IPv4
    server_address.sin_port = htons(PORT);             // Port in network byte order
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);  // Convert IP to binary form

    // Try to connect to the server
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        cerr << "Connection to server failed.\n";
        return 1;
    }

    // Get user info
    string username, room;
    cout << "Enter your username: ";
    getline(cin, username);  // Read full username line
    cout << "Enter room name to join: ";
    getline(cin, room);      // Read full room name

    // Send username and room name to server (in two separate messages)
    send(sock, username.c_str(), username.length(), 0);
    sleep(1); // Wait 1 second to give server time to read the first message
              // (not ideal; better to use a delimiter or structured protocol)
    send(sock, room.c_str(), room.length(), 0);

    // Start a new thread to continuously receive messages from the server
    thread(receive_messages, sock).detach();

    // Loop to read user input and send to server
    string message;
    cout << "> "; // Display prompt
    while (getline(cin, message)) {
        // Send message to server
        send(sock, message.c_str(), message.length(), 0);
        cout << "> ";  // Re-display prompt
    }

    // When user exits input loop (e.g., Ctrl+D), close the socket
    close(sock);
    return 0;
}

