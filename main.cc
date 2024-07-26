#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include "channel.h"

void log(const std::string& message) {
    printf("[%llu] %s\n",
           static_cast<unsigned long long>(
               std::hash<std::thread::id>{}(std::this_thread::get_id())),
           message.c_str());
}

void test_basic_functionality() {
    log("Testing basic functionality");
    Channel<int> channel;
    channel.send(1);
    auto received = channel.receive();
    assert(received && *received == 1);
    log("Basic send and receive successful");
}

void test_capacity_limit() {
    log("Testing capacity limit");
    Channel<int> channel(5);  // Bounded channel with capacity 5

    for (int i = 0; i < 5; ++i) {
        channel.send(i);
        log("Sent value: " + std::to_string(i));
    }

    std::thread blocker([&channel]() {
        log("Attempting to send to full channel");
        auto start = std::chrono::steady_clock::now();
        channel.send(100);
        auto end = std::chrono::steady_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        log("Send to full channel completed after " +
            std::to_string(duration.count()) + "ms");
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    log("Receiving from full channel");
    auto received = channel.receive();
    assert(received && *received == 0);

    blocker.join();
    log("Capacity limit test completed");
}

void test_multiple_producers_consumers() {
    log("Testing multiple producers and consumers");
    Channel<int> channel(10);  // Bounded channel with capacity 10
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < 3; ++i) {
        producers.emplace_back([&channel, i]() {
            for (int j = 0; j < 5; ++j) {
                channel.send(i * 10 + j);
                log("Producer " + std::to_string(i) +
                    " sent: " + std::to_string(i * 10 + j));
            }
        });
    }

    for (int i = 0; i < 2; ++i) {
        consumers.emplace_back([&channel, i]() {
            for (int j = 0; j < 7; ++j) {
                auto received = channel.receive();
                assert(received);
                log("Consumer " + std::to_string(i) +
                    " received: " + std::to_string(*received));
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    log("Multiple producers and consumers test completed");
}

void test_closing_channel() {
    log("Testing channel closure");
    Channel<int> channel;
    std::thread sender([&channel]() {
        for (int i = 0; i < 3; ++i) {
            channel.send(i);
            log("Sent value before closure: " + std::to_string(i));
        }
        channel.close();
        log("Channel closed");
        try {
            channel.send(100);
        } catch (const std::runtime_error& e) {
            log("Caught expected exception: " + std::string(e.what()));
        }
    });

    std::thread receiver([&channel]() {
        while (true) {
            auto received = channel.receive();
            if (!received) {
                log("Received end of channel");
                break;
            }
            log("Received value: " + std::to_string(*received));
        }
    });

    sender.join();
    receiver.join();
    log("Channel closure test completed");
}

void test_unbounded_channel() {
    log("Testing unbounded channel");
    Channel<int> channel;  // Unbounded channel

    std::thread sender([&channel]() {
        for (int i = 0; i < 1000; ++i) {
            channel.send(i);
            if (i % 100 == 0) {
                log("Sent value: " + std::to_string(i));
            }
        }
        log("Finished sending to unbounded channel");
    });

    std::thread receiver([&channel]() {
        for (int i = 0; i < 1000; ++i) {
            auto received = channel.receive();
            assert(received && *received == i);
            if (i % 100 == 0) {
                log("Received value: " + std::to_string(*received));
            }
        }
        log("Finished receiving from unbounded channel");
    });

    sender.join();
    receiver.join();
    log("Unbounded channel test completed");
}

int main() {
    log("Starting Channel tests");

    test_basic_functionality();
    test_capacity_limit();
    test_multiple_producers_consumers();
    test_closing_channel();
    test_unbounded_channel();

    log("All tests completed successfully");
    return 0;
}