# Finance Tracker C++ Backend
# Optimized build with -O2 for release performance.

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pedantic
TARGET   = finance_server
SRC      = back.cpp
BUILD_DIR = build

.PHONY: all run clean test dirs

all: dirs $(BUILD_DIR)/$(TARGET)

dirs:
	@mkdir -p $(BUILD_DIR) data

$(BUILD_DIR)/$(TARGET): $(SRC) http_server.hpp store.hpp models.hpp json_util.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BUILD_DIR)/$(TARGET)

run: all
	@./build/finance_server

clean:
	rm -rf $(BUILD_DIR)

test: all
	@chmod +x test_api.sh
	@./test_api.sh
