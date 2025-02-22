CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
INCLUDES = -I./src

# Define build directory
BUILD_DIR = build
SRC_DIR = src

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
TARGET = $(BUILD_DIR)/proxy_server

.PHONY: all clean directories

all: directories $(TARGET)

directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p logs

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(CXXFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) 