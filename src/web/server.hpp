#pragma once
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
#endif

class SimpleHTTPServer {
private:
    std::atomic<bool> running{false};
    std::thread server_thread;
    int port{8080};
    
public:
    SimpleHTTPServer(int p = 8080) : port(p) {}
    
    void start() {
        running = true;
        server_thread = std::thread(&SimpleHTTPServer::run, this);
        std::cout << "HTTP Server started on port " << port << std::endl;
    }
    
    void stop() {
        running = false;
        if (server_thread.joinable()) {
            server_thread.join();
        }
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
        
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
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
        
        std::cout << "Server listening on http://localhost:" << port << std::endl;
        
        while (running) {
            int addrlen = sizeof(address);
            int client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
            
            if (client_fd < 0) {
                continue;
            }
            
            // Handle request
            handle_client(client_fd);
            
#ifdef _WIN32
            closesocket(client_fd);
#else
            close(client_fd);
#endif
        }
        
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
    }
    
    void handle_client(int client_fd) {
        char buffer[1024] = {0};
        
#ifdef _WIN32
        recv(client_fd, buffer, sizeof(buffer), 0);
#else
        read(client_fd, buffer, sizeof(buffer));
#endif
        
        // Simple response
        std::string response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<html><body>"
            "<h1>Trading Dashboard</h1>"
            "<p>Fast Trading Terminal v1.0</p>"
            "<p>Status: Running</p>"
            "<p>Visit the console for detailed output</p>"
            "</body></html>";
        
#ifdef _WIN32
        send(client_fd, response.c_str(), response.length(), 0);
#else
        write(client_fd, response.c_str(), response.length());
#endif
    }
};