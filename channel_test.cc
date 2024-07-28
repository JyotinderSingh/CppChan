#include "channel.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

void log(const std::string& message) {
    printf("[%llu] %s\n",
           static_cast<unsigned long long>(
               std::hash<std::thread::id>{}(std::this_thread::get_id())),
           message.c_str());
}

void test_buffered_channel() {
    log("Testing buffered channel");
    Channel<int> ch(2);

    log("Sending 1 and 2 (should not block)");
    ch.send(1);
    ch.send(2);
    log("Sent 1 and 2 successfully");

    log("Trying to send 3 (should block)");
    std::thread blocker([&ch] {
        auto start = std::chrono::steady_clock::now();
        ch.send(3);
        auto end = std::chrono::steady_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        log("Sent 3 after blocking for " + std::to_string(duration.count()) +
            "ms");
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    log("Receiving 1");
    assert(*ch.receive() == 1 && "Failed to receive 1");
    blocker.join();

    log("Receiving remaining values");
    assert(*ch.receive() == 2 && "Failed to receive 2");
    assert(*ch.receive() == 3 && "Failed to receive 3");

    log("Buffered channel test completed");
}

void test_unbuffered_channel() {
    log("Testing unbuffered channel");
    Channel<int> ch;

    std::thread sender([&ch] {
        log("Sending 1 (should block until received)");
        auto start = std::chrono::steady_clock::now();
        ch.send(1);
        auto end = std::chrono::steady_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        log("Sent 1 after blocking for " + std::to_string(duration.count()) +
            "ms");
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    log("Receiving 1");
    assert(*ch.receive() == 1 && "Failed to receive 1");
    sender.join();

    log("Unbuffered channel test completed");
}

void test_async_operations() {
    log("Testing asynchronous operations");
    Channel<int> ch(1);

    log("Asynchronously sending 1");
    auto future_send = ch.async_send(1);

    log("Asynchronously receiving");
    auto future_recv = ch.async_receive();

    log("Waiting for async receive to complete");
    assert(*future_recv.get() == 1 && "Async receive failed");
    log("Async receive completed");

    log("Waiting for async send to complete");
    future_send.wait();
    log("Async send completed");

    log("Asynchronous operations test completed");
}

void test_try_operations() {
    log("Testing try_send and try_receive");
    Channel<int> ch(1);

    log("Trying to send 1 (should succeed)");
    assert(ch.try_send(1) && "Failed to send 1");

    log("Trying to send 2 (should fail as channel is full)");
    assert(!ch.try_send(2) && "Unexpectedly succeeded in sending 2");

    log("Trying to receive (should succeed)");
    auto received = ch.try_receive();
    assert(received && *received == 1 && "Failed to receive 1");

    log("Trying to receive again (should fail as channel is empty)");
    assert(!ch.try_receive() && "Unexpectedly succeeded in receiving");

    log("Try operations test completed");
}

void test_close_operations() {
    log("Testing close operations");
    Channel<int> ch(1);

    ch.send(1);
    log("Sent 1 to channel");

    log("Closing channel");
    ch.close();

    log("Attempting to send on closed channel (should throw an exception)");
    try {
        ch.send(2);
        assert(false && "Expected exception was not thrown");
    } catch (const std::runtime_error& e) {
        log("Caught expected exception: " + std::string(e.what()));
    }

    log("Receiving from closed channel");
    auto value = ch.receive();
    assert(value && *value == 1 && "Failed to receive 1 from closed channel");
    log("Received 1 from closed channel");

    log("Receiving again from closed and empty channel");
    value = ch.receive();
    assert(!value && "Unexpectedly received a value from empty closed channel");
    log("Received nullopt as expected");

    log("Close operations test completed");
}

void test_multiple_producers_consumers() {
    log("Testing multiple producers and consumers");
    Channel<int> ch(10);

    const int num_producers = 3;
    const int num_consumers = 2;
    const int items_per_producer = 5;
    const int total_items = num_producers * items_per_producer;

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    std::atomic<int> items_produced(0);
    std::atomic<int> items_consumed(0);

    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&ch, &items_produced, i] {
            for (int j = 0; j < items_per_producer; ++j) {
                ch.send(i * 100 + j);
                log("Producer " + std::to_string(i) +
                    " sent: " + std::to_string(i * 100 + j));
                items_produced++;
            }
        });
    }

    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&ch, &items_consumed, i] {
            while (items_consumed < total_items) {
                auto received = ch.receive();
                if (received) {
                    log("Consumer " + std::to_string(i) +
                        " received: " + std::to_string(*received));
                    items_consumed++;
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    log("All producers finished");

    // Wait for all items to be consumed
    while (items_consumed < total_items) {
        std::this_thread::yield();
    }

    ch.close();
    log("Channel closed");

    for (auto& t : consumers) t.join();
    log("All consumers finished");

    assert(items_produced == total_items &&
           "Incorrect number of items produced");
    assert(items_consumed == total_items &&
           "Incorrect number of items consumed");
    assert(ch.is_empty() && "Channel is not empty after test");
    log("Multiple producers and consumers test completed");
}

int main() {
    log("Starting Channel tests");

    test_buffered_channel();
    test_unbuffered_channel();
    test_async_operations();
    test_try_operations();
    test_close_operations();
    test_multiple_producers_consumers();

    log("All tests completed successfully");
    return 0;
}