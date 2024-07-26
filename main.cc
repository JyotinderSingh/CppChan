#include <chrono>
#include <iostream>
#include <thread>

#include "channel.h"

void log(const std::string& message) {
    std::cout << "[" << std::this_thread::get_id() << "] " << message
              << std::endl;
}

int main() {
    log("Creating a buffered channel with capacity 2");
    Channel<int> ch(2);

    log("Demonstrating synchronous send and receive");
    std::thread sender([&ch] {
        log("Sender thread started");
        log("Sending 1");
        ch.send(1);
        log("Sending 2");
        ch.send(2);
        log("Sending 3 (this will block until a receive happens)");
        ch.send(3);
        log("Sent 3 successfully");
    });

    std::thread receiver([&ch] {
        log("Receiver thread started");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        for (int i = 1; i <= 3; ++i) {
            auto value = ch.receive();
            log("Received: " + std::to_string(*value));
        }
    });

    sender.join();
    receiver.join();

    log("\nDemonstrating asynchronous send and receive");
    log("Asynchronously sending 4");
    auto future_send = ch.async_send(4);
    log("Asynchronously receiving");
    auto future_recv = ch.async_receive();

    log("Waiting for async receive to complete");
    auto received_value = future_recv.get();
    log("Async receive completed, value: " + std::to_string(*received_value));

    log("Waiting for async send to complete");
    future_send.wait();
    log("Async send completed");

    log("\nDemonstrating non-blocking send and receive");
    if (ch.try_send(5)) {
        log("Non-blocking send successful: Sent 5");
    } else {
        log("Non-blocking send failed: Channel full or closed");
    }

    if (auto value = ch.try_receive()) {
        log("Non-blocking receive successful: Received " +
            std::to_string(*value));
    } else {
        log("Non-blocking receive failed: Channel empty or closed");
    }

    log("\nClosing the channel");
    ch.close();

    log("Attempting to send on closed channel (should throw an exception)");
    try {
        ch.send(6);
    } catch (const std::runtime_error& e) {
        log("Caught exception: " + std::string(e.what()));
    }

    log("Receiving from closed channel");
    auto final_receive = ch.receive();
    if (final_receive) {
        log("Received final value: " + std::to_string(*final_receive));
    } else {
        log("Channel is empty and closed");
    }

    return 0;
}