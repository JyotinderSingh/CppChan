#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>

#include "channel.h"

std::atomic<bool> should_stop(false);

void log(const std::string& message) {
    printf("[%llu] %s\n",
           static_cast<unsigned long long>(
               std::hash<std::thread::id>{}(std::this_thread::get_id())),
           message.c_str());
}

void int_producer(Channel<int>& ch, int id, int count) {
    for (int i = 0; i < count && !should_stop; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 500));
        int value = id * 1000 + i;
        if (ch.try_send(value)) {
            log("Int Producer " + std::to_string(id) +
                " sent: " + std::to_string(value));
        } else {
            log("Int Producer " + std::to_string(id) +
                " failed to send: " + std::to_string(value));
        }
    }
}

void string_producer(Channel<std::string>& ch, int id, int count) {
    for (int i = 0; i < count && !should_stop; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 500));
        std::string value =
            "Message " + std::to_string(id) + "-" + std::to_string(i);
        if (ch.try_send(value)) {
            log("String Producer " + std::to_string(id) + " sent: " + value);
        } else {
            log("String Producer " + std::to_string(id) +
                " failed to send: " + value);
        }
    }
}

void consumer(Channel<int>& ch_int, Channel<std::string>& ch_str) {
    Selector selector;

    selector.add_receive<int>(ch_int, [](int value) {
        log("Received int: " + std::to_string(value));
    });

    selector.add_receive<std::string>(ch_str, [](const std::string& value) {
        log("Received string: " + value);
    });

    selector.select(should_stop);

    log("Consumer stopped");
}

int main() {
    Channel<int> ch_int(5);
    Channel<std::string> ch_str(5);

    std::thread cons([&ch_int, &ch_str] { consumer(ch_int, ch_str); });

    std::thread prod1([&ch_int] { int_producer(ch_int, 1, 20); });
    std::thread prod2([&ch_int] { int_producer(ch_int, 2, 20); });
    std::thread prod3([&ch_str] { string_producer(ch_str, 3, 20); });

    prod1.join();
    prod2.join();
    prod3.join();

    // Close the channels before signaling the consumer to stop
    ch_int.close();
    ch_str.close();

    // Signal consumer to stop
    should_stop = true;

    // Wait for consumer to finish
    cons.join();

    return 0;
}