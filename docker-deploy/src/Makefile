CXX = g++
CXXFLAGS = -g -Wall -std=c++17 -lpthread

TARGET = main

SRCS = main.cpp \
       CacheManager.cpp \
       ConnectionHandler.cpp \
       HttpParser.cpp \
       Logger.cpp \
       MessageForwarder.cpp \
       ProxyServer.cpp \
       RequestHandler.cpp \

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

main.o: main.cpp ProxyServer.h Logger.h
ProxyServer.o: ProxyServer.cpp ProxyServer.h Logger.h
Logger.o: Logger.cpp Logger.h
CacheManager.o: CacheManager.cpp CacheManager.h
ConnectionHandler.o: ConnectionHandler.cpp ConnectionHandler.h
HttpParser.o: HttpParser.cpp HttpParser.h
MessageForwarder.o: MessageForwarder.cpp MessageForwarder.h
RequestHandler.o: RequestHandler.cpp RequestHandler.h
Response.o: Response.hpp

.PHONY: clean
clean:
	rm -rf $(OBJS) $(TARGET)
