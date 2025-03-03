#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

class ProxyClient {
private:
    std::string host;
    int port;
    int sock;

public:
    ProxyClient(const std::string& host, int port) : host(host), port(port), sock(-1) {}
    
    bool connect() {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return false;

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(host.c_str());

        return ::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) >= 0;
    }

    bool sendRequest(const std::string& request) {
        return send(sock, request.c_str(), request.length(), 0) > 0;
    }
    std::string receiveResponse() {
        std::string response;
        char buffer[4096];
        int bytesRead;
        while ((bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytesRead] = '\0';  // Null terminate the received data
            response += buffer;
            
            if (response.find("\r\n\r\n") != std::string::npos) {
                break;
            }
        }

        if (bytesRead == 0) {
            return response;
        }
        else if (bytesRead < 0) {
            return "";
        }

        return response;
    }

    bool establishSSLTunnel(const std::string& host, int port) {
        // Create connect request
        std::string connect_request = 
            "CONNECT " + host + ":" + std::to_string(port) + " HTTP/1.1\r\n"
            "Host: " + host + ":" + std::to_string(port) + "\r\n"
            "Proxy-Connection: Keep-Alive\r\n\r\n";
        // Send request to the server
        if (!sendRequest(connect_request)) {
            return false;
        }
        
        std::string response = receiveResponse();
        // std::cout << response << std::endl;
        return response.find("200 Connection Established") != std::string::npos;
    }

    int getSocket() const {
        return sock;
    }

    ~ProxyClient() {
        if (sock >= 0) {
            close(sock);
        }
    }
};

// Test GET request
void testHttpGet() {
    std::cout << "\n=== Testing HTTP GET Request ===" << std::endl;
    try {
        ProxyClient client("127.0.0.1", 12345);
        
        if (!client.connect()) {
            std::cout << "Failed to connect to proxy server" << std::endl;
            return;
        }

        // Test with a non-HTTPS website that accepts GET requests
        std::string request = 
            "GET http://httpbin.org/get HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "Connection: close\r\n"
            "\r\n";

        if (!client.sendRequest(request)) {
            std::cout << "Failed to send GET request" << std::endl;
            return;
        }

        std::string response = client.receiveResponse();
        std::cout << "GET Response:\n" << response << std::endl;
        
        // Verify response
        if (response.find("200 OK") != std::string::npos) {
            std::cout << "GET test passed!" << std::endl;
        } else {
            std::cout << "GET test failed!" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in GET test: " << e.what() << std::endl;
    }
}

// Test POST request
void testHttpPost() {
    std::cout << "\n=== Testing HTTP POST Request ===" << std::endl;
    try {
        ProxyClient client("127.0.0.1", 12345);
        
        if (!client.connect()) {
            std::cout << "Failed to connect to proxy server" << std::endl;
            return;
        }

        // Create POST data
        std::string postData = "test_data=Hello+World";
        
        std::string request = 
            "POST http://httpbin.org/post HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: " + std::to_string(postData.length()) + "\r\n"
            "Connection: close\r\n"
            "\r\n" +
            postData;

        if (!client.sendRequest(request)) {
            std::cout << "Failed to send POST request" << std::endl;
            return;
        }

        std::string response = client.receiveResponse();
        std::cout << "POST Response:\n" << response << std::endl;
        
        // Verify response
        if (response.find("200 OK") != std::string::npos && 
            response.find("test_data") != std::string::npos) {
            std::cout << "POST test passed!" << std::endl;
        } else {
            std::cout << "POST test failed!" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in POST test: " << e.what() << std::endl;
    }
}

// Test CONNECT method (HTTPS)
void testHttpsConnect() {
    std::cout << "\n=== Testing HTTPS CONNECT Method ===" << std::endl;
    try {
        // create proxy client
        ProxyClient client("127.0.0.1", 12345);
        if (!client.connect()) {
            std::cout << "Failed to connect to proxy server" << std::endl;
            return;
        }

        const std::string host = "www.google.com";
        const int port = 443;

        if (!client.establishSSLTunnel(host, port)) {
            std::cout << "Failed to establish SSL tunnel" << std::endl;
            return;
        }

        // Initialize OpenSSL
        SSL_library_init();
        SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) {
            std::cout << "Failed to create SSL context" << std::endl;
            return;
        }

        // Create SSL connection
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client.getSocket());
        
        // Perform SSL handshake
        if (SSL_connect(ssl) <= 0) {
            std::cout << "Failed SSL handshake" << std::endl;
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            return;
        }

        // Send HTTPS request
        std::string request = 
            "GET / HTTP/1.1\r\n"
            "Host: " + host + "\r\n"
            "Connection: close\r\n\r\n";

        SSL_write(ssl, request.c_str(), request.length());

        // Receive response
        char buffer[4096];
        std::string response;
        int bytes;
        while ((bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes] = '\0';
            response += buffer;
        }

        std::cout << "HTTPS Response:\n" << response << std::endl;

        // Clean up
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        
    } catch (const std::exception& e) {
        std::cerr << "Error in CONNECT test: " << e.what() << std::endl;
    }
}

// Test real-world scenario (Google Search)
void testGoogleSearch() {
    std::cout << "\n=== Testing Google Search (HTTPS) ===" << std::endl;
    try {
        ProxyClient client("127.0.0.1", 12345);
        
        if (!client.connect()) {
            std::cout << "Failed to connect to proxy server" << std::endl;
            return;
        }

        if (!client.establishSSLTunnel("www.google.com", 443)) {
            std::cout << "Failed to establish SSL tunnel to Google" << std::endl;
            return;
        }

        std::cout << "Successfully established HTTPS connection to Google!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error in Google Search test: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "Starting proxy client tests..." << std::endl;

    // Run tests
    testHttpGet();
    testHttpPost();
    testHttpsConnect();
    testGoogleSearch();

    return 0;
}