http-caching-proxy/
│── src/
│   │── main.cpp                # Entry point for the proxy server
│   │── ProxyServer.cpp         # Core proxy server logic
│   │── ProxyServer.h           # Proxy server class declaration
│   │── RequestHandler.cpp      # Handles incoming HTTP requests
│   │── RequestHandler.h        # Request handler class
│   │── CacheManager.cpp        # Caching logic implementation
│   │── CacheManager.h          # Cache management class
│   │── Logger.cpp              # Logging functionality
│   │── Logger.h                # Logger class declaration
│   │── ConnectionHandler.cpp   # Manages multiple client connections
│   │── ConnectionHandler.h     # Connection handler class
│   │── HttpParser.cpp          # Parses HTTP requests and responses
│   │── HttpParser.h            # HTTP parser class
│
│── include/                    # Header files (if needed separately)
│   │── ProxyServer.h
│   │── RequestHandler.h
│   │── CacheManager.h
│   │── Logger.h
│   │── ConnectionHandler.h
│   │── HttpParser.h
│
│── tests/
│   │── test_cache.cpp          # Unit tests for caching
│   │── test_proxy.cpp          # Unit tests for proxy server
│   │── test_requests.cpp       # Tests for request handling
│   │── malformed_request.txt   # Sample malformed request for testing
│   │── valid_request.txt       # Sample valid request for testing
│
│── logs/                       # Logs directory (mounted in Docker)
│   │── proxy.log               # Proxy log file (output)
│
│── config/
│   │── proxy.conf              # Configuration file for proxy settings
│
│── docker/
│   │── Dockerfile              # Docker image setup
│   │── docker-compose.yml      # Docker Compose setup
│
│── scripts/
│   │── run_tests.sh            # Script to run tests
│   │── start_proxy.sh          # Script to start the proxy server
│
│── .gitignore                  # Git ignore file
│── README.md                   # Project description and setup guide
│── LICENSE                     # License file
│── Makefile                     # Makefile for compiling the project
