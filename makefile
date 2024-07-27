CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
HEADERS = channel.h channel.cc
TEST_SOURCES = channel_test.cc selector_test.cc
BUILD_DIR = build
TEST_EXECUTABLES = $(addprefix $(BUILD_DIR)/, $(TEST_SOURCES:.cc=))

all: $(TEST_EXECUTABLES)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/channel_test: channel_test.cc $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/selector_test: selector_test.cc $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

test: $(TEST_EXECUTABLES)
	@echo "Running channel_test..."
	@$(BUILD_DIR)/channel_test
	@echo "\nRunning selector_test..."
	@$(BUILD_DIR)/selector_test

test_channel: $(BUILD_DIR)/channel_test
	@echo "Running channel_test..."
	@$(BUILD_DIR)/channel_test

test_selector: $(BUILD_DIR)/selector_test
	@echo "Running selector_test..."
	@$(BUILD_DIR)/selector_test

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean test test_channel test_selector