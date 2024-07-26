#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>

template <typename T>
class Channel {
   public:
    Channel(size_t cap = 0) : capacity(cap) {}

    void send(const T &value) {
        std::unique_lock<std::mutex> lock(mtx);
        if (closed) {
            throw std::runtime_error("Send on closed channel");
        }
        cv.wait(lock,
                [this] { return queue.size() < capacity || capacity == 0; });
        queue.push(value);
        cv.notify_one();
    }

    std::optional<T> receive() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !queue.empty() || closed; });
        if (queue.empty() && closed) {
            return std::nullopt;
        }
        T value = queue.front();
        queue.pop();
        cv.notify_one();
        return value;
    }

    void close() {
        std::unique_lock<std::mutex> lock(mtx);
        closed = true;
        cv.notify_all();
    }

    bool is_closed() const {
        std::unique_lock<std::mutex> lock(mtx);
        return closed;
    }

   private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool closed = false;
    size_t capacity;
};