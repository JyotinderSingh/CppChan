#include "channel.h"

template <typename T>
void Channel<T>::send(const T& value) {
    std::unique_lock<std::mutex> lock(mtx);
    if (closed) {
        throw std::runtime_error("Send on closed channel");
    }
    if (capacity == 0) {
        // For unbuffered channels, wait until there's a receiver or the channel
        // is closed
        cv_send.wait(lock, [this] { return waitingReceivers > 0 || closed; });
        if (closed) {
            throw std::runtime_error("Channel closed while waiting to send");
        }
        --waitingReceivers;
        queue.push(value);
        cv_recv.notify_one();  // Notify a waiting receiver
        // Notify all registered selectors
        for (auto selector : selectors) {
            selector->notify();
        }
    } else {
        // For buffered channels, wait until there's space in the buffer or the
        // channel is closed
        cv_send.wait(lock,
                     [this] { return queue.size() < capacity || closed; });
        if (closed) {
            throw std::runtime_error("Channel closed while waiting to send");
        }
        queue.push(value);
        cv_recv.notify_one();  // Notify a waiting receiver
        // Notify all registered selectors
        for (auto selector : selectors) {
            selector->notify();
        }
    }
}

template <typename T>
std::future<void> Channel<T>::async_send(const T& value) {
    // Launch an asynchronous task to send the value
    return std::async(std::launch::async, [this, value] { this->send(value); });
}

template <typename T>
bool Channel<T>::try_send(const T& value) {
    std::unique_lock<std::mutex> lock(mtx);
    // If the channel is closed or the buffer is full, return false
    if (closed || (capacity != 0 && queue.size() >= capacity)) {
        return false;
    }
    queue.push(value);
    cv_recv.notify_one();  // Notify a waiting receiver
    // Notify all registered selectors
    for (auto selector : selectors) {
        selector->notify();
    }
    return true;
}

template <typename T>
std::optional<T> Channel<T>::receive() {
    std::unique_lock<std::mutex> lock(mtx);
    if (capacity == 0) {
        // For unbuffered channels, notify a sender and wait for a value
        ++waitingReceivers;
        cv_send.notify_one();
        cv_recv.wait(lock, [this] { return !queue.empty() || closed; });
        --waitingReceivers;
    } else {
        // For buffered channels, wait until there's a value or the channel is
        // closed
        cv_recv.wait(lock, [this] { return !queue.empty() || closed; });
    }
    if (queue.empty() && closed) {
        return std::nullopt;  // Return empty optional if channel is closed and
                              // empty
    }
    T value = queue.front();
    queue.pop();
    cv_send.notify_one();  // Notify a waiting sender
    return value;
}

template <typename T>
std::future<std::optional<T>> Channel<T>::async_receive() {
    // Launch an asynchronous task to receive a value
    return std::async(std::launch::async, [this] { return this->receive(); });
}

template <typename T>
std::optional<T> Channel<T>::try_receive() {
    std::unique_lock<std::mutex> lock(mtx);
    if (queue.empty()) {
        return std::nullopt;  // Return empty optional if queue is empty
    }
    T value = queue.front();
    queue.pop();
    cv_send.notify_one();  // Notify a waiting sender
    return value;
}

template <typename T>
void Channel<T>::close() {
    std::unique_lock<std::mutex> lock(mtx);
    closed = true;
    cv_send.notify_all();  // Notify all waiting senders
    cv_recv.notify_all();  // Notify all waiting receivers
    // Notify all registered selectors
    for (auto selector : selectors) {
        selector->notify();
    }
}

template <typename T>
void Channel<T>::register_selector(Selector* selector) {
    std::unique_lock<std::mutex> lock(mtx);
    selectors.push_back(selector);
}

template <typename T>
void Channel<T>::unregister_selector(Selector* selector) {
    std::unique_lock<std::mutex> lock(mtx);
    // Remove the selector from the list
    selectors.erase(std::remove(selectors.begin(), selectors.end(), selector),
                    selectors.end());
}

template <typename T>
void Selector::add_receive(Channel<T>& ch, std::function<void(T)> callback) {
    std::unique_lock<std::mutex> lock(mtx);
    ch.register_selector(this);
    // Add a lambda function to the channels list
    channels.push_back([&ch, callback = std::move(callback), this]() mutable {
        if (ch.is_closed()) {
            ch.unregister_selector(this);
            return true;  // Signal that this channel is done
        }
        auto value = ch.try_receive();
        if (value) {
            callback(*value);  // Call the callback with the received value
            return true;
        }
        return false;
    });
}

bool Selector::select() {
    std::unique_lock<std::mutex> lock(mtx);

    // Try to find a channel that has data available
    auto it = std::find_if(channels.begin(), channels.end(),
                           [](const auto& ch) { return ch(); });
    if (it != channels.end()) {
        return true;
    }

    if (channels.empty()) {
        return false;  // No channels to select from
    }

    // Wait until a channel has data available
    cv.wait(lock, [this] {
        return std::any_of(channels.begin(), channels.end(),
                           [](const auto& ch) { return ch(); });
    });

    // Find the channel that has data and remove it from the list
    it = std::find_if(channels.begin(), channels.end(),
                      [](const auto& ch) { return ch(); });
    if (it != channels.end()) {
        channels.erase(it);
    }
    return it != channels.end();
}