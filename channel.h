#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>

template <typename T>
class Channel {
   public:
    /**
     * @brief Constructs a Channel object.
     * @param cap The capacity of the channel. If 0, creates an unbuffered
     * channel.
     *
     * Use Case: Create buffered or unbuffered channels for inter-thread
     * communication. Example: Channel<int> ch(5); // Creates a buffered channel
     * with capacity 5 Channel<std::string> ch; // Creates an unbuffered channel
     */
    Channel(size_t cap = 0) : capacity(cap) {}

    /**
     * @brief Sends a value to the channel. Blocks if the channel is full
     * (buffered) or if there's no receiver (unbuffered).
     * @param value The value to send.
     * @throws std::runtime_error if the channel is closed.
     *
     * Use Case: Send data to other threads in a blocking manner.
     * Example: ch.send(42);
     */
    void send(const T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        if (closed) {
            throw std::runtime_error("Send on closed channel");
        }
        if (capacity == 0) {
            // Unbuffered channel: wait for a receiver
            cv.wait(lock, [this] { return waitingReceivers > 0 || closed; });
            if (closed)
                throw std::runtime_error(
                    "Channel closed while waiting to send");
            --waitingReceivers;
            queue.push(value);
            cv.notify_one();
        } else {
            // Buffered channel: wait if the buffer is full
            cv.wait(lock, [this] { return queue.size() < capacity || closed; });
            if (closed)
                throw std::runtime_error(
                    "Channel closed while waiting to send");
            queue.push(value);
            cv.notify_one();
        }
    }

    /**
     * @brief Asynchronously sends a value to the channel.
     * @param value The value to send.
     * @return A std::future<void> representing the asynchronous operation.
     *
     * Use Case: Send data without blocking the current thread.
     * Example: auto future = ch.async_send(42);
     *          future.wait(); // Wait for the send to complete if needed
     */
    std::future<void> async_send(const T& value) {
        return std::async(std::launch::async,
                          [this, value] { this->send(value); });
    }

    /**
     * @brief Attempts to send a value to the channel without blocking.
     * @param value The value to send.
     * @return true if the value was sent, false otherwise.
     *
     * Use Case: Try to send data without blocking, useful for timeouts or
     * polling. Example: if (ch.try_send(42)) { std::cout << "Sent
     * successfully\n";
     *          }
     */
    bool try_send(const T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        if (closed || (capacity != 0 && queue.size() >= capacity)) {
            return false;
        }
        queue.push(value);
        cv_recv.notify_one();
        return true;
    }

    /**
     * @brief Receives a value from the channel. Blocks if the channel is empty.
     * @return An optional containing the received value, or std::nullopt if the
     * channel is closed and empty.
     *
     * Use Case: Receive data from other threads in a blocking manner.
     * Example: auto value = ch.receive();
     *          if (value) {
     *              std::cout << "Received: " << *value << "\n";
     *          }
     */
    std::optional<T> receive() {
        std::unique_lock<std::mutex> lock(mtx);
        if (capacity == 0) {
            // Unbuffered channel: notify sender and wait for value
            ++waitingReceivers;
            cv.notify_one();
            cv.wait(lock, [this] { return !queue.empty() || closed; });
            --waitingReceivers;
        } else {
            // Buffered channel: wait if the buffer is empty
            cv.wait(lock, [this] { return !queue.empty() || closed; });
        }
        if (queue.empty() && closed) {
            return std::nullopt;
        }
        T value = queue.front();
        queue.pop();
        cv.notify_one();
        return value;
    }

    /**
     * @brief Asynchronously receives a value from the channel.
     * @return A std::future containing an optional with the received value.
     *
     * Use Case: Receive data without blocking the current thread.
     * Example: auto future = ch.async_receive();
     *          auto value = future.get();
     */
    std::future<std::optional<T>> async_receive() {
        return std::async(std::launch::async,
                          [this] { return this->receive(); });
    }

    /**
     * @brief Attempts to receive a value from the channel without blocking.
     * @return An optional containing the received value, or std::nullopt if the
     * channel is empty.
     *
     * Use Case: Try to receive data without blocking, useful for timeouts or
     * polling. Example:
     * if (auto value = ch.try_receive()) {
     *  std::cout << "Received: " << *value << "\n";
     * }
     */
    std::optional<T> try_receive() {
        std::unique_lock<std::mutex> lock(mtx);
        if (queue.empty()) {
            return std::nullopt;
        }
        T value = queue.front();
        queue.pop();
        cv_send.notify_one();
        return value;
    }

    /**
     * @brief Closes the channel. No more values can be sent after closing.
     *
     * Use Case: Signal that no more values will be sent on this channel.
     * Example: ch.close();
     */
    void close() {
        std::unique_lock<std::mutex> lock(mtx);
        closed = true;
        cv_send.notify_all();
        cv_recv.notify_all();
    }

    /**
     * @brief Checks if the channel is closed.
     * @return true if the channel is closed, false otherwise.
     *
     * Use Case: Check the channel state before sending or in a loop.
     * Example:
     * while (!ch.is_closed()) {
     *  // Perform operations
     * }
     */
    bool is_closed() const {
        std::unique_lock<std::mutex> lock(mtx);
        return closed;
    }

    /**
     * @brief Checks if the channel is empty.
     * @return true if the channel is empty, false otherwise.
     *
     * Use Case: Check if there are any pending values in the channel.
     * Example:
     * if (!ch.is_empty()) {
     *  auto value = ch.receive();
     * }
     */
    bool is_empty() const {
        std::unique_lock<std::mutex> lock(mtx);
        return queue.empty();
    }

    /**
     * @brief Returns the current number of items in the channel.
     * @return The number of items currently in the channel.
     *
     * Use Case: Check how many items are waiting in the channel.
     * Example: std::cout << "Items in channel: " << ch.size() << "\n";
     */
    size_t size() const {
        std::unique_lock<std::mutex> lock(mtx);
        return queue.size();
    }

   private:
    std::queue<T> queue;
    mutable std::mutex mtx;
    std::condition_variable cv, cv_send, cv_recv;
    bool closed = false;
    size_t capacity;
    size_t waitingReceivers = 0;
};