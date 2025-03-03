#include "MessageForwarder.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>

void MessageForwarder::forwardGet(HttpRequest& req, int clientSocket, int clientId, std::shared_ptr<Logger> logger) {
    // Log the request before forwarding
    logger->log("Requesting \"" + req.request + " from " + req.host, clientId);
    
    // Generate cache key
    std::string cacheKey = generateCacheKey(req);
    
    // Check in the cache
    auto cacheIt = responseCache.find(cacheKey);
    //
    //logger->log("Finding Cache key: " + cacheKey, clientId);
    // pritn every pair in the cache
    //for (auto it = responseCache.begin(); it != responseCache.end(); ++it) {
       // logger->log("Has Cache key: " + it->first, clientId);
    //}
    bool fromCache = false;
    bool revalidationNeeded = false;
    if (cacheIt == responseCache.end()){
        // print to logfile: ID: not in cache
        logger->log("not in cache", clientId); // wks
    }
    if (cacheIt != responseCache.end()) {
        // Check if cache entry is still valid
        if (time(nullptr) < cacheIt->second.expiration && !cacheIt->second.mustRevalidate) {
            // Get from cache
            // print to logfile: ID: in cache, valid
            logger->log("in cache, valid", clientId); // wks
            //logger->log(Logger::LogLevel::INFO, "Serving response from cache for: " + req.host + req.request, clientId);
            send(clientSocket, cacheIt->second.response.c_str(), cacheIt->second.response.length(), 0);
            return;
        }
        else if (time(nullptr) >= cacheIt->second.expiration) // wks
        {
            logger->log("in cache, but expired at " + std::to_string(cacheIt->second.expiration), clientId); // wks
        }
        else if (!cacheIt->second.etag.empty() || !cacheIt->second.lastModified.empty()) {
            // Need to revalidate(走协商缓存)
            revalidationNeeded = true;
            logger->log("in cache, requires validation", clientId); // wks
            // Add validation headers to the request
            if (!cacheIt->second.etag.empty()) {
                req.headers["If-None-Match"] = cacheIt->second.etag;
            }
            if (!cacheIt->second.lastModified.empty()) {
                req.headers["If-Modified-Since"] = cacheIt->second.lastModified;
            }
        }
    }
    
    //logger->log("Requesting \"" + req.request + "\" from " + req.host, clientId); // wks
    
    // Connect to the target server
    int serverSocket = connectToServer(req.host, req.port);
    if (serverSocket < 0) {
        logger->log(Logger::LogLevel::ERROR, "Failed to connect to server: " + req.host + ":" + req.port, clientId);
        sendErrorResponse(clientSocket, 502, "Bad Gateway");
        return;
    }
    
    // Forward the request to the server
    std::string requestToSend = buildForwardRequest(req);
    //logger->log("1111111", clientId);
    //logger->log(Logger::LogLevel::INFO, requestToSend.c_str(), clientId);
    
    if (send(serverSocket, requestToSend.c_str(), requestToSend.length(), 0) < 0) {
        logger->log(Logger::LogLevel::ERROR, "Failed to send request to server", clientId);
        close(serverSocket);
        sendErrorResponse(clientSocket, 500, "Internal Server Error");
        return;
    }
    
    // Read and forward the response from the server to the client
    bool keepAliveServer = false;
    bool keepAliveClient = false;
    
    // Check if client requested keep-alive
    auto clientConnectionIt = req.headers.find("Connection");
    if (clientConnectionIt != req.headers.end() && 
        strcasecmp(clientConnectionIt->second.c_str(), "keep-alive") == 0) {
        keepAliveClient = true;
    }
    
    // Buffer for receiving data
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    std::string responseHeaders;
    std::string fullResponse; // For caching
    bool headersComplete = false;
    size_t contentLength = 0;
    size_t receivedBodyBytes = 0;
    bool chunkedEncoding = false;
    
    // Read and process the response
    while ((bytesRead = recv(serverSocket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        
        // Store the full response for potential caching
        fullResponse.append(buffer, bytesRead);
        
        // If Proxy hasn't finished reading headers
        if (!headersComplete) {
            responseHeaders.append(buffer, bytesRead);
            
            // Check if we've received all headers
            size_t headerEnd = responseHeaders.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                headersComplete = true;
                std::string responseLine = responseHeaders.substr(0, responseHeaders.find("\r\n")); // wks
                //logger->log(Logger::LogLevel::INFO, std::to_string(clientId) + ": Received \"" + responseLine + "\" from " + req.host, clientId); // wks
                
                // Handle 304 Not Modified for cache revalidation
                if (revalidationNeeded && responseHeaders.find("HTTP/1.1 304") != std::string::npos) {
                    logger->log(Logger::LogLevel::INFO, "Revalidated cache - serving from cache", clientId);
                    
                    // Update expiration time
                    CacheEntry& entry = responseCache[cacheKey];
                    entry.expiration = getExpirationTime(responseHeaders);
                    
                    // Serve from cache
                    send(clientSocket, entry.response.c_str(), entry.response.length(), 0);
                    fromCache = true;
                    break;
                }
                
                // Extract headers
                std::string headerSection = responseHeaders.substr(0, headerEnd);
                
                // Check if server supports keep-alive
                if (headerSection.find("Connection: keep-alive") != std::string::npos) {
                    keepAliveServer = true;
                }
                
                // Read the content length
                size_t contentLengthPos = headerSection.find("Content-Length: ");
                if (contentLengthPos != std::string::npos) {
                    size_t valueStart = contentLengthPos + 16; // Length of "Content-Length: "
                    size_t valueEnd = headerSection.find("\r\n", valueStart);
                    std::string lengthStr = headerSection.substr(valueStart, valueEnd - valueStart);
                    contentLength = std::stoul(lengthStr);
                }
                
                // Check for chunked encoding
                if (headerSection.find("Transfer-Encoding: chunked") != std::string::npos) {
                    chunkedEncoding = true;
                }
                
                // Calculate how much of the body we've already received
                receivedBodyBytes = responseHeaders.length() - (headerEnd + 4); // +4 for \r\n\r\n
                
                // Send the headers to the client
                if (send(clientSocket, responseHeaders.c_str(), responseHeaders.length(), 0) < 0) {
                    logger->log(Logger::LogLevel::ERROR, "Failed to send response headers to client", clientId);
                    break;
                }
                size_t newlinePos = responseHeaders.find('\n');
                std::string responseL = responseHeaders.substr(0, newlinePos);
                logger->log("Responding \"" + responseL.substr(0, responseL.size()-1) + "\"", clientId);
                
                // If we already received all data, exit the loop
                if ((contentLength > 0 && receivedBodyBytes >= contentLength) || 
                    (contentLength == 0 && !chunkedEncoding)) {
                    break;
                }
            }
        } else {
            // Send the body to the client
            if (send(clientSocket, buffer, bytesRead, 0) < 0) {
                logger->log(Logger::LogLevel::ERROR, "Failed to send response body to client", clientId);
                break;
            }
            
            receivedBodyBytes += bytesRead;
            
            // If we've received all data, exit the loop
            if (contentLength > 0 && receivedBodyBytes >= contentLength) {
                break;
            }
            
            // For chunked encoding, look for the end chunk marker "0\r\n\r\n"
            if (chunkedEncoding) {
                std::string chunk(buffer, bytesRead);
                if (chunk.find("0\r\n\r\n") != std::string::npos) {
                    break;
                }
            }
        }
    }
    

    // Handle read errors or connection closed by server
    if (bytesRead < 0) {
        logger->log(Logger::LogLevel::ERROR, "Error reading response from server: " + std::string(strerror(errno)), clientId);
    }
    //when it receives the response from the origin server, it should print: ID: Received"RESPONSE" fromSERVER
    size_t newlinePos = fullResponse.find('\n');
    std::string responseLine = fullResponse.substr(0, newlinePos);
    logger->log("Received \"" + responseLine.substr(0, responseLine.size()-1) + "\" from " + req.host, clientId);
    // Handle caching if the response wasn't served from cache
    
    
    if (!fromCache && headersComplete) {
        
        if (isCacheable("GET", responseHeaders)) {
            
            //logger->log(Logger::LogLevel::INFO, "Caching response for: " + req.host + req.request, clientId);
            
            CacheEntry entry;
            entry.response = fullResponse;
            entry.expiration = getExpirationTime(responseHeaders);
            entry.mustRevalidate = checkMustRevalidate(responseHeaders);
            extractValidationHeaders(responseHeaders, entry.etag, entry.lastModified);
            
            // print cacheKey
            //logger->log("Storing Cache key: " + generateCacheKey(req), clientId);
            responseCache[generateCacheKey(req)] = entry;
            //for (auto it = responseCache.begin(); it != responseCache.end(); ++it) {
                //logger->log("Has Cache key: " + it->first, clientId);
            //}
        }
    }
    
    // Close the server connection if keep-alive is not supported/requested
    if (!keepAliveServer) {
        close(serverSocket);
    } else {
        // Store the connection for future use
        saveKeepAliveConnection(req.host, req.port, serverSocket);
    }
    
    //logger->log(Logger::LogLevel::INFO, "Completed forwarding GET request for client " + std::to_string(clientId), clientId);
    
}

/*
@biref: Helper function to build the forwarded request
*/
std::string MessageForwarder::buildForwardRequest(const HttpRequest& req) {
    std::stringstream ss;
    
    // Build the request line
    ss <<  req.request << "\r\n";
    
    // Add headers
    for (const auto& header : req.headers) {
        // Skip hop-by-hop headers
        /*
        */
        if (strcasecmp(header.first.c_str(), "Connection") == 0){
            continue;
        }
         //   strcasecmp(header.first.c_str(), "Keep-Alive") == 0 ||
         //   strcasecmp(header.first.c_str(), "Proxy-Connection") == 0 ||
         //   strcasecmp(header.first.c_str(), "Proxy-Authorization") == 0 ||
         //   strcasecmp(header.first.c_str(), "TE") == 0 ||
         //   strcasecmp(header.first.c_str(), "Trailer") == 0 ||
          //  strcasecmp(header.first.c_str(), "Transfer-Encoding") == 0 ||
          //  strcasecmp(header.first.c_str(), "Upgrade") == 0) {
          //  continue;
        //}
        ss << header.first << ": " << header.second << "\r\n";
    }
    
    // Web proxy <---> target server (Always Keep-alive)
    ss << "Connection: close\r\n";
    
    // End of headers
    ss << "\r\n";
    
    return ss.str();
}

/*
@brief: Helper function to connect to the target server
*/
 int MessageForwarder::connectToServer(const std::string& host, const std::string& port) {
    // First check if we already have a keep-alive connection
    int existingSocket = getKeepAliveConnection(host, port);
    if (existingSocket > 0) {
        // Test if the connection is still valid
        char test;
        if (recv(existingSocket, &test, 1, MSG_PEEK | MSG_DONTWAIT) == 0) {
            close(existingSocket);
            removeKeepAliveConnection(host, port);
        } else {
            return existingSocket;
        }
    }
    
    // Create a new connection
    struct addrinfo hints, *res;
    int sockfd;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) {
        return -1;
    }
    
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(res);
        return -1;
    }
    // set socket
    //setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));    
    // Non-blocking mode
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    // Attempt to connect
    int connectResult = connect(sockfd, res->ai_addr, res->ai_addrlen);
    if (connectResult < 0) {
        if (errno == EINPROGRESS) {
            fd_set fdset;
            struct timeval tv;
            
            FD_ZERO(&fdset);
            FD_SET(sockfd, &fdset);
            // 5 s Time out
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            if (select(sockfd + 1, NULL, &fdset, NULL, &tv) == 1) {
                int so_error;
                socklen_t len = sizeof so_error;
                getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
                
                if (so_error != 0) {
                    close(sockfd);
                    freeaddrinfo(res);
                    return -1;
                }
            } else {
                // Error
                close(sockfd);
                freeaddrinfo(res);
                return -1;
            }
        } else {
            close(sockfd);
            freeaddrinfo(res);
            return -1;
        }
    }
    // Blocking mode
    fcntl(sockfd, F_SETFL, flags);
    freeaddrinfo(res);
    return sockfd;
}

/*
 @brief: function to send an error response to the client
*/
 void MessageForwarder::sendErrorResponse(int clientSocket, int statusCode, const std::string& statusText) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    ss << "Content-Type: text/html\r\n";
    ss << "Connection: close\r\n";
    
    std::string body = "<html><body><h1>" + std::to_string(statusCode) + " " + statusText + "</h1></body></html>";
    
    ss << "Content-Length: " << body.length() << "\r\n";
    ss << "\r\n";
    ss << body;
    
    std::string response = ss.str();
    send(clientSocket, response.c_str(), response.length(), 0);
}

/*
@ brief: Helper function to get a keep-alive connection
*/
int MessageForwarder::getKeepAliveConnection(const std::string& host, const std::string& port) {
    std::string key = host + ":" + port;
    std::lock_guard<std::mutex> guard(keepAliveMutex); 
    auto it = keepAliveConnections.find(key);
    if (it != keepAliveConnections.end()) {
        return it->second;
    }
    return -1;
}
/*
  @biref: Helper function to save a keep-alive connection
*/
void MessageForwarder::saveKeepAliveConnection(const std::string& host, const std::string& port, int socket) {
    std::string key = host + ":" + port;
    // Thrad safe
    std::lock_guard<std::mutex> guard(keepAliveMutex);
    
    // If there was an old connection, close it first
    auto it = keepAliveConnections.find(key);
    if (it != keepAliveConnections.end()) {
        close(it->second);
    }
    
    keepAliveConnections[key] = socket;
}

void MessageForwarder::removeKeepAliveConnection(const std::string& host, const std::string& port) {
    std::string key = host + ":" + port;
    std::lock_guard<std::mutex> guard(keepAliveMutex);
    
    keepAliveConnections.erase(key);
}

void MessageForwarder::forwardPost(HttpRequest& req, int clientSocket, int clientId, std::shared_ptr<Logger> logger) {
    //logger->log(Logger::LogLevel::INFO, "Forwarding POST request for client " + std::to_string(clientId) + ": " + req.url);
    
    //Connect to the target server
    std::string port = req.port.empty() ? "80" : req.port;
    int serverSocket = connectToServer(req.host, port);
    
    if (serverSocket < 0) {
        logger->log(Logger::LogLevel::ERROR, "Failed to connect to server: " + req.host + ":" + port);
        sendErrorResponse(clientSocket, 502, "Bad Gateway");
        return;
    }
    
    //Check Content-Length header
    size_t contentLength = 0;
    auto contentLengthIt = req.headers.find("Content-Length");
    if (contentLengthIt != req.headers.end()) {
        try {
            contentLength = std::stoul(contentLengthIt->second);
        } catch (const std::exception& e) {
            logger->log(Logger::LogLevel::ERROR, "Invalid Content-Length: " + contentLengthIt->second);
            close(serverSocket);
            sendErrorResponse(clientSocket, 400, "Bad Request");
            return;
        }
    }
    
    // Check for chunked encoding
    bool chunkedEncoding = false;
    auto transferEncodingIt = req.headers.find("Transfer-Encoding");
    if (transferEncodingIt != req.headers.end() && 
        transferEncodingIt->second.find("chunked") != std::string::npos) {
        chunkedEncoding = true;
    }
    
    // If don't have Content-Length don't have chunked encoding 
    if (contentLength == 0 && !chunkedEncoding && !req.body.empty()) {
        logger->log(Logger::LogLevel::ERROR, "POST request without proper Content-Length or Transfer-Encoding");
        close(serverSocket);
        sendErrorResponse(clientSocket, 400, "Bad Request");
        return;
    }
    
    // Build the request to forward
    std::string requestToSend = buildForwardRequest(req);
    
    // For POST requests, we need to append the body
    requestToSend += req.body;
    
    // Send the request to the server
    if (send(serverSocket, requestToSend.c_str(), requestToSend.length(), 0) < 0) {
        logger->log(Logger::LogLevel::ERROR, "Failed to send POST request to server: " + std::string(strerror(errno)));
        close(serverSocket);
        sendErrorResponse(clientSocket, 500, "Internal Server Error");
        return;
    }
    
    if (chunkedEncoding && req.body.find("0\r\n\r\n") == std::string::npos) {
        //logger->log(Logger::LogLevel::DEBUG, "Reading additional chunked data from client");
        
        char buffer[BUFFER_SIZE];
        bool chunkedComplete = false;
        
        while (!chunkedComplete) {
            ssize_t bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
            
            if (bytesRead <= 0) {
                if (bytesRead < 0) {
                    logger->log(Logger::LogLevel::ERROR, "Error reading chunked data from client: " + std::string(strerror(errno)));
                } else {
                    logger->log(Logger::LogLevel::ERROR, "Client closed connection while reading chunked data");
                }
                close(serverSocket);
                return;
            }
            
            buffer[bytesRead] = '\0';
            std::string chunk(buffer, bytesRead);
            
            //Forward the chunk to the server
            if (send(serverSocket, chunk.c_str(), chunk.length(), 0) < 0) {
                logger->log(Logger::LogLevel::ERROR, "Failed to forward chunk to server: " + std::string(strerror(errno)));
                close(serverSocket);
                return;
            }
            
            //Check if this is the last chunk
            if (chunk.find("0\r\n\r\n") != std::string::npos) {
                chunkedComplete = true;
            }
        }
    }
    
    //Read and forward the response from the server to the client
    bool keepAliveServer = false;
    bool keepAliveClient = false;
    
    //Check if client requested keep-alive
    auto clientConnectionIt = req.headers.find("Connection");
    if (clientConnectionIt != req.headers.end() && 
        strcasecmp(clientConnectionIt->second.c_str(), "keep-alive") == 0) {
        keepAliveClient = true;
    }
    
    //Process server response
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    std::string responseHeaders;
    bool headersComplete = false;
    size_t responseContentLength = 0;
    size_t receivedBodyBytes = 0;
    bool responseChunked = false;
    
    //Read and process the response
    while ((bytesRead = recv(serverSocket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        
        if (!headersComplete) {
            responseHeaders.append(buffer, bytesRead);
            
            size_t headerEnd = responseHeaders.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                headersComplete = true;
                
                //Extract headers to check for keep-alive and content length
                std::string headerSection = responseHeaders.substr(0, headerEnd);
                
                //Check if server supports keep-alive
                if (headerSection.find("Connection: keep-alive") != std::string::npos) {
                    keepAliveServer = true;
                }
                
                //Check for Content-Length
                size_t contentLengthPos = headerSection.find("Content-Length: ");
                if (contentLengthPos != std::string::npos) {
                    size_t valueStart = contentLengthPos + 16; // Length of "Content-Length: "
                    size_t valueEnd = headerSection.find("\r\n", valueStart);
                    std::string lengthStr = headerSection.substr(valueStart, valueEnd - valueStart);
                    responseContentLength = std::stoul(lengthStr);
                }
                
                //Check for chunked encoding
                if (headerSection.find("Transfer-Encoding: chunked") != std::string::npos) {
                    responseChunked = true;
                }
                
                //Calculate how much of the body we've already received
                receivedBodyBytes = responseHeaders.length() - (headerEnd + 4); 
                
                //Send the complete headers and any part of the body we've received to the client
                if (send(clientSocket, responseHeaders.c_str(), responseHeaders.length(), 0) < 0) {
                    logger->log(Logger::LogLevel::ERROR, "Failed to send response headers to client");
                    break;
                }
                //Whenever your proxy responds to the client, it should log: ID: Responding "RESPONSE"
                size_t newlinePos = responseHeaders.find('\n');
                std::string responseLine = responseHeaders.substr(0, newlinePos);
                logger->log("Responding \"" + responseLine + "\"", clientId);
                
                if ((responseContentLength > 0 && receivedBodyBytes >= responseContentLength) || 
                    (responseContentLength == 0 && !responseChunked)) {
                    break;
                }
            }
        } else {
            if (send(clientSocket, buffer, bytesRead, 0) < 0) {
                logger->log(Logger::LogLevel::ERROR, "Failed to send response body to client");
                break;
            }
            
            receivedBodyBytes += bytesRead;
        
            if (responseContentLength > 0 && receivedBodyBytes >= responseContentLength) {
                break;
            }
            
            if (responseChunked) {
                std::string chunk(buffer, bytesRead);
                if (chunk.find("0\r\n\r\n") != std::string::npos) {
                    break;
                }
            }
        }
    }
    
    //Handle read errors or connection closed by server
    if (bytesRead < 0) {
        logger->log(Logger::LogLevel::ERROR, "Error reading response from server: " + std::string(strerror(errno)));
    }
    
    //Close the server connection if keep-alive is not supported/requested
    if (!keepAliveServer) {
        close(serverSocket);
    } else {
        // Store the connection for future use
        saveKeepAliveConnection(req.host, port, serverSocket);
    }
    
    //logger->log(Logger::LogLevel::INFO, "Completed forwarding POST request for client " + std::to_string(clientId));
}
    
void MessageForwarder::forwardConnect(HttpRequest& req, int clientSocket, int clientId, std::shared_ptr<Logger> logger) {
    //logger->log(Logger::INFO, "Handling CONNECT request for client " + std::to_string(clientId) + ": " + req.host + ":" + req.port, clientId);
    
    //Connect to the target server
    int serverSocket = connectToServer(req.host, req.port);
    if (serverSocket < 0) {
        logger->log(Logger::ERROR, "Failed to connect to server: " + req.host + ":" + req.port, clientId);
        sendErrorResponse(clientSocket, 502, "Bad Gateway");
        return;
    }
    
    //Send 200 Connection Established response to the client
    std::string response = "HTTP/1.1 200 Connection Established\r\n";
    response += "Proxy-Agent: MyProxy/1.0\r\n";
    response += "\r\n";
    
    if (send(clientSocket, response.c_str(), response.length(), 0) < 0) {
        logger->log(Logger::ERROR, "Failed to send Connection Established response to client", clientId);
        close(serverSocket);
        return;
    }
    
    //Set up for tunneling data between client and server
    fd_set readFds;
    char buffer[BUFFER_SIZE];
    bool tunnelActive = true;
    
    //Make both sockets non-blocking for the tunneling
    int clientFlags = fcntl(clientSocket, F_GETFL, 0);
    int serverFlags = fcntl(serverSocket, F_GETFL, 0);
    fcntl(clientSocket, F_SETFL, clientFlags | O_NONBLOCK);
    fcntl(serverSocket, F_SETFL, serverFlags | O_NONBLOCK);
    
    logger->log(Logger::LogLevel::INFO, "Established tunnel for client " + std::to_string(clientId) + " to " + req.host + ":" + req.port, clientId);
    
    // Tunnel Loop
    while (tunnelActive) {
        FD_ZERO(&readFds);
        FD_SET(clientSocket, &readFds);
        FD_SET(serverSocket, &readFds);
        
        int maxFd = std::max(clientSocket, serverSocket) + 1;
        struct timeval timeout;
        timeout.tv_sec = 30; 
        timeout.tv_usec = 0;
        
        int activity = select(maxFd, &readFds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            if (errno == EINTR) {
                continue;
            }
            logger->log(Logger::LogLevel::ERROR, "Select error in tunnel: " + std::string(strerror(errno)), clientId);
            break;
        }
        
        if (activity == 0) {
            //logger->log(Logger::LogLevel::DEBUG, "Tunnel timeout for client " + std::to_string(clientId) + ", checking connection", clientId);
            //TODO: test here
            continue;
        }
        
        // Check if client has data to read
        if (FD_ISSET(clientSocket, &readFds)) {
            ssize_t bytesRead = recv(clientSocket, buffer, BUFFER_SIZE, 0);
            
            if (bytesRead <= 0) {
                // Client closed connection or error
                if (bytesRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    // Non-blocking operation would block, try again later
                    continue;
                }
                logger->log(Logger::LogLevel::INFO, "Client " + std::to_string(clientId) + " closed connection or error occurred", clientId);
                tunnelActive = false;
                break;
            }
            
            // Forward data to server
            ssize_t bytesSent = 0;
            ssize_t totalBytesSent = 0;
            
            while (totalBytesSent < bytesRead) {
                bytesSent = send(serverSocket, buffer + totalBytesSent, bytesRead - totalBytesSent, 0);
                
                if (bytesSent <= 0) {
                    if (bytesSent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        fd_set writeFds;
                        FD_ZERO(&writeFds);
                        FD_SET(serverSocket, &writeFds);
                        
                        timeout.tv_sec = 5; 
                        timeout.tv_usec = 0;
                        
                        if (select(serverSocket + 1, NULL, &writeFds, NULL, &timeout) <= 0) {
                            // Timeout or error
                            tunnelActive = false;
                            break;
                        }
                        continue;
                    }
                    
                    // Error occurred
                    logger->log(Logger::LogLevel::ERROR, "Error sending data to server: " + std::string(strerror(errno)), clientId);
                    tunnelActive = false;
                    break;
                }
                
                totalBytesSent += bytesSent;
            }
            
            if (!tunnelActive) {
                break;
            }
        }
        
        // Check if server has data to read
        if (FD_ISSET(serverSocket, &readFds)) {
            ssize_t bytesRead = recv(serverSocket, buffer, BUFFER_SIZE, 0);
            
            if (bytesRead <= 0) {
                // Server closed connection or error
                if (bytesRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    // Non-blocking operation would block, try again later
                    continue;
                }
                
                logger->log(Logger::LogLevel::INFO, "Server closed connection or error occurred");
                tunnelActive = false;
                break;
            }
            
            // Forward data to client
            ssize_t bytesSent = 0;
            ssize_t totalBytesSent = 0;
            
            while (totalBytesSent < bytesRead) {
                bytesSent = send(clientSocket, buffer + totalBytesSent, bytesRead - totalBytesSent, 0);
                
                if (bytesSent <= 0) {
                    if (bytesSent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        fd_set writeFds;
                        FD_ZERO(&writeFds);
                        FD_SET(clientSocket, &writeFds);
                        
                        timeout.tv_sec = 5;  // 5 seconds timeout for write
                        timeout.tv_usec = 0;
                        
                        if (select(clientSocket + 1, NULL, &writeFds, NULL, &timeout) <= 0) {
                            // Timeout or error
                            tunnelActive = false;
                            break;
                        }
                        continue;
                    }
                    
                    // Error occurred
                    logger->log(Logger::LogLevel::ERROR, "Error sending data to client: " + std::string(strerror(errno)));
                    tunnelActive = false;
                    break;
                }
                
                totalBytesSent += bytesSent;
            }
            
            if (!tunnelActive) {
                break;
            }
        }
    }
    
    // Clean up
    close(serverSocket);
    logger->log("Tunnel closed", clientId);
    
}

/****CACHE****/

/*
@biref: Generate cache key from request
*/
std::string MessageForwarder::generateCacheKey(const HttpRequest& req) {
    return req.host + req.port + req.request;
}



/*
@biref: Check if a response is cacheable
*/
bool MessageForwarder::isCacheable(const std::string& method, const std::string& responseHeaders) {
    // Only cache GET responses
    if (method != "GET") {
        return false;
    }
    
    // Don't cache if Cache-Control: no-store
    if (responseHeaders.find("Cache-Control: no-store") != std::string::npos) {
        return false;
    }
    
    // Don't cache if private (for shared cache)
    if (responseHeaders.find("Cache-Control: private") != std::string::npos) {
        return false;
    }
    
    // Other status code checks (only cache 200, 203, 300, 301, etc.)
    std::string statusLine = responseHeaders.substr(0, responseHeaders.find("\r\n"));
    if (statusLine.find(" 200 ") == std::string::npos && 
        statusLine.find(" 203 ") == std::string::npos && 
        statusLine.find(" 300 ") == std::string::npos && 
        statusLine.find(" 301 ") == std::string::npos) {
        return false;
    }
    return true;
}

/*
@biref: Extract expiration time from headers
*/
time_t MessageForwarder::getExpirationTime(const std::string& responseHeaders) {
    time_t expiresAt = time(nullptr) + 60; // Default: 60 seconds
    
    // Check for Cache-Control: max-age
    size_t maxAgePos = responseHeaders.find("Cache-Control: max-age=");
    if (maxAgePos != std::string::npos) {
        size_t valueStart = maxAgePos + 22; // Length of "Cache-Control: max-age="
        size_t valueEnd = responseHeaders.find("\r\n", valueStart);
        if (valueEnd == std::string::npos) {
            valueEnd = responseHeaders.find(",", valueStart);
        }
        if (valueEnd != std::string::npos) {
            std::string maxAgeStr = responseHeaders.substr(valueStart, valueEnd - valueStart);
            try {
                int maxAge = std::stoi(maxAgeStr);
                expiresAt = time(nullptr) + maxAge;
            } catch (const std::exception& e) {
                // Handle parsing error
            }
        }
    } else {
        // Check for Expires header if no max-age
        size_t expiresPos = responseHeaders.find("Expires: ");
        if (expiresPos != std::string::npos) {
            size_t valueStart = expiresPos + 9; // Length of "Expires: "
            size_t valueEnd = responseHeaders.find("\r\n", valueStart);
            if (valueEnd != std::string::npos) {
                std::string expiresStr = responseHeaders.substr(valueStart, valueEnd - valueStart);
                struct tm tm = {};
                // RFC 7231 date format: "Tue, 01 Jan 2019 00:00:00 GMT"
                if (strptime(expiresStr.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm) != nullptr) {
                    expiresAt = mktime(&tm);
                }
            }
        }
    }
    
    return expiresAt;
}


/*
    @brief: Extract cache validation headers
*/
void MessageForwarder::extractValidationHeaders(const std::string& responseHeaders, std::string& etag, std::string& lastModified) {
    // Extract ETag
    size_t etagPos = responseHeaders.find("ETag: ");
    if (etagPos != std::string::npos) {
        size_t valueStart = etagPos + 6; // Length of "ETag: "
        size_t valueEnd = responseHeaders.find("\r\n", valueStart);
        if (valueEnd != std::string::npos) {
            etag = responseHeaders.substr(valueStart, valueEnd - valueStart);
        }
    }
    
    // Extract Last-Modified
    size_t lastModifiedPos = responseHeaders.find("Last-Modified: ");
    if (lastModifiedPos != std::string::npos) {
        size_t valueStart = lastModifiedPos + 15; // Length of "Last-Modified: "
        size_t valueEnd = responseHeaders.find("\r\n", valueStart);
        if (valueEnd != std::string::npos) {
            lastModified = responseHeaders.substr(valueStart, valueEnd - valueStart);
        }
    }
}
/*
@biref: check whether need revalidate
*/
bool MessageForwarder::checkMustRevalidate(const std::string& responseHeaders) {
    return responseHeaders.find("Cache-Control: must-revalidate") != std::string::npos ||
           responseHeaders.find("Cache-Control: no-cache") != std::string::npos;
}