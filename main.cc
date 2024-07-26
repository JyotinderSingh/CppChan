#include <iostream>
#include <thread>
#include <vector>

#include "channel.h"

void producer(Channel<int> &ch, int start, int end) {
    for (int i = start; i <= end; ++i) {
        ch.send(i);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));  // Simulate work
    }
}

void consumer(Channel<int> &ch, int id) {
    while (true) {
        auto value = ch.receive();
        if (!value) {  // Channel is closed
            printf("Consumer %d exiting\n", id);
            break;
        }
        printf("Consumer %d received: %d\n", id, *value);
    }
}

int main() {
    Channel<int> ch(5);  // Create a channel with capacity 5

    std::thread prod1(producer, std::ref(ch), 1, 10);
    std::thread prod2(producer, std::ref(ch), 11, 20);

    std::vector<std::thread> consumers;
    for (int i = 0; i < 3; ++i) {
        consumers.emplace_back(consumer, std::ref(ch), i + 1);
    }

    prod1.join();
    prod2.join();

    ch.close();

    for (auto &t : consumers) {
        t.join();
    }

    return 0;
}