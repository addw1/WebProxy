#include <string>

#include "HttpParser.h"

class MessageForwarder {
public:
    // Forward GET request
    std::string forwardGet(HttpRequest& req);
    // Forward POST request
    std::string forwardPost(HttpRequest& req);
    // Forward CONNECT request
    std::string forwardConnect(HttpRequest& req, int clientSock);
private:
    std::string forwardRequest(HttpRequest& req, const std::string& port);
};