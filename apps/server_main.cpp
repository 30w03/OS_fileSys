#include "network/server.h"
#include "filesystem/filesystem.h"
#include <iostream>
#include <csignal>
#include <memory>

std::unique_ptr<Server> g_server;

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down server..." << std::endl;
        if (g_server) {
            g_server->stop();
        }
        exit(0);
    }
}

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -p, --port PORT        Server port (default: 8080)" << std::endl;
    std::cout << "  -d, --disk IMAGE       Disk image file (default: disk.img)" << std::endl;
    std::cout << "  -h, --help             Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    std::string diskImage = "disk.img";
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
            } else {
                std::cerr << "Error: --port requires an argument" << std::endl;
                return 1;
            }
        } else if (arg == "-d" || arg == "--disk") {
            if (i + 1 < argc) {
                diskImage = argv[++i];
            } else {
                std::cerr << "Error: --disk requires an argument" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        std::cout << "Starting Peer Review System Server..." << std::endl;
        std::cout << "Port: " << port << std::endl;
        std::cout << "Disk image: " << diskImage << std::endl;
        
        // Create and start server
        g_server = std::make_unique<Server>(port, diskImage);
        
        if (!g_server->start()) {
            std::cerr << "Failed to start server" << std::endl;
            return 1;
        }
        
        std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;
        
        // Run server
        g_server->run();
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
