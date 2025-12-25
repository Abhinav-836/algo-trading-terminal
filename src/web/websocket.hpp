#pragma once
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #ifndef socklen_t
        typedef int socklen_t;
    #endif
#endif

class SimpleWebSocket {
private:
    std::atomic<bool> running{false};
    std::thread ws_thread;
    int port{8081};
    int server_fd{-1};
    
public:
    SimpleWebSocket(int p = 8081) : port(p) {}
    
    ~SimpleWebSocket() {
        stop();
    }
    
    void start() {
        running = true;
        ws_thread = std::thread(&SimpleWebSocket::run, this);
        std::cout << "WebSocket server started on port " << port << std::endl;
    }
    
    void stop() {
        running = false;
        if (ws_thread.joinable()) {
            ws_thread.join();
        }
        
        if (server_fd != -1) {
#ifdef _WIN32
            closesocket(server_fd);
            WSACleanup();
#else
            close(server_fd);
#endif
        }
    }
    
    void broadcast(const std::string& message) {
        // Simple broadcast implementation
        // In production, would maintain list of connected clients
        std::cout << "[WebSocket] Broadcast: " << message << std::endl;
    }
    
private:
    void run() {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return;
        }
#endif
        
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "Socket creation failed" << std::endl;
            return;
        }
        
        int opt = 1;
#ifdef _WIN32
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Bind failed" << std::endl;
            return;
        }
        
        if (listen(server_fd, 3) < 0) {
            std::cerr << "Listen failed" << std::endl;
            return;
        }
        
        std::cout << "WebSocket listening on ws://localhost:" << port << std::endl;
        
        while (running) {
            int addrlen = sizeof(address);
            int client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
            
            if (client_fd < 0) {
                continue;
            }
            
            // Handle WebSocket handshake
            handle_handshake(client_fd);
            
            // Simple echo server for now
            char buffer[1024] = {0};
#ifdef _WIN32
            recv(client_fd, buffer, sizeof(buffer), 0);
#else
            read(client_fd, buffer, sizeof(buffer));
#endif
            
            std::cout << "WebSocket message: " << buffer << std::endl;
            
#ifdef _WIN32
            closesocket(client_fd);
#else
            close(client_fd);
#endif
        }
    }
    
    void handle_handshake(int client_fd) {
        char buffer[1024] = {0};
        
#ifdef _WIN32
        recv(client_fd, buffer, sizeof(buffer), 0);
#else
        read(client_fd, buffer, sizeof(buffer));
#endif
        
        std::string request(buffer);
        
        // Simple WebSocket handshake
        if (request.find("Upgrade: websocket") != std::string::npos) {
            std::string response = 
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "\r\n";
            
#ifdef _WIN32
            send(client_fd, response.c_str(), response.length(), 0);
#else
            write(client_fd, response.c_str(), response.length());
#endif
            
            std::cout << "WebSocket handshake completed" << std::endl;
        }
    }
};