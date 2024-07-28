# CppChan: Go-inspired Concurrency Primitive

CppChan is a high-performance, thread-safe channel in C++17, inspired by Go's channels. It provides a flexible communication mechanism for concurrent programming, supporting both buffered and unbuffered channels with synchronous and asynchronous operations.

It also supports a selector mechanism to wait on multiple channels simultaneously, allowing you to handle incoming data from multiple sources efficiently.

## Features

- Templated design for any data type
- Buffered and unbuffered channels
- Synchronous and asynchronous operations
- Blocking and non-blocking send/receive
- RAII-compliant resource handling
- Thread-safe closure mechanism
- Selector for waiting on multiple channels

## Installation

To use it in your project, simply include the `channel.h` file:

```cpp
#include "channel.h"
```

Ensure you're compiling with C++17 support (-std=c++17 flag for most compilers).

## Build

```bash
make
```

## Test

```bash
make test
```

## Thread Safety

All operations on the Channel are thread-safe. Multiple threads can safely send to and receive from the same channel concurrently.

## Error Handling

- Sending to a closed channel will throw a `std::runtime_error`.
- Receiving from a closed channel will return `std::nullopt`.

## Performance Considerations

- Unbuffered channels provide stronger synchronization but may have higher overhead.
- Buffered channels can improve performance by reducing synchronization, but be mindful of the buffer size to avoid excessive memory usage.
- Use non-blocking operations (`try_send` and `try_receive`) when appropriate to avoid potential deadlocks.

## API Reference

### Instantiation

```cpp
// Contructor
Channel(size_t capacity = 0)
```

Creates a new channel.

- capacity: The buffer size. If 0, creates an unbuffered channel.

Use case: Create buffered or unbuffered channels for inter-thread communication.

Example

```cpp
Channel<int> bufferedChan(5);  // Buffered channel with capacity 5
Channel<std::string> unbufferedChan;  // Unbuffered channel
```

### Sending Operations

#### Synchronous Send

```cpp
void send(const T& value)
```

Sends a value to the channel. Blocks if the channel is full (buffered) or if there's no receiver (unbuffered).

Use case: Send data to other threads in a blocking manner.

Example

```cpp
ch.send(42);
```

#### Asynchronous Send

```cpp
std::future<void> async_send(const T& value)
```

Asynchronously sends a value to the channel.

Use case: Send data without blocking the current thread.

Example

```cpp
auto future = ch.async_send(42);
future.wait();  // Wait for the send to complete if needed
```

#### Non-blocking Send

```cpp
bool try_send(const T& value)
```

Attempts to send a value to the channel without blocking.

Use case: Try to send data without blocking, useful for timeouts or polling.

Example

```cpp
if (ch.try_send(42)) {
    std::cout << "Sent successfully\n";
}
```

### Receiving Operations

#### Synchronous Receive

```cpp
std::optional<T> receive()
```

Receives a value from the channel. Blocks if the channel is empty.

Use case: Receive data from other threads in a blocking manner.

Example

```cpp
auto value = ch.receive();
if (value) {
  std::cout << "Received: " << *value << "\n";
}
```

#### Asynchronous Receive

```cpp
std::future<std::optional<T>> async_receive()
```

Asynchronously receives a value from the channel.

Use case: Receive data without blocking the current thread.

Example

```cpp
auto future = ch.async_receive();
auto value = future.get(); // Wait for the receive to complete
```

#### Non-blocking Receive

```cpp
std::optional<T> try_receive()
```

Attempts to receive a value from the channel without blocking.

Use case: Try to receive data without blocking, useful for timeouts or polling.

Example

```cpp
if (auto value = ch.try_receive()) {
    std::cout << "Received: " << *value << "\n";
}
```

### Channel Management

#### Close

```cpp
void close()
```

Closes the channel. No more values can be sent after closing.

Use case: Signal that no more values will be sent on this channel.

Example

```cpp
ch.close();
```

#### is_closed()

```cpp
bool is_closed() const
```

Checks if the channel is closed.

Use case: Check the channel state before sending or in a loop.

Example

```cpp
while (!ch.is_closed()) {
    // Perform operations
}
```

#### is_empty()

```cpp
bool is_empty() const
```

Checks if the channel is empty.

Use case: Check if there are any pending values to receive.

Example

```cpp
if (!ch.is_empty()) {
    auto value = ch.receive();
}
```

#### size()

```cpp
size_t size() const
```

Returns the current number of items in the channel.

Use case: Check how many items are waiting in the channel.

Example

```cpp
std::cout << "Items in channel: " << ch.size() << "\n";
```

### Selector Operations

The `Selector` class allows you to wait on multiple channels and execute callbacks when data is received.

#### Add Channel to Selector

```cpp
template <typename T>
void add_receive(Channel<T>& ch, std::function<void(T)> callback)
```

Registers a channel and its receive callback with the selector.

Use case: Wait on multiple channels and handle incoming data.

Example

```cpp
Selector selector;
selector.add_receive<std::string>(ch, [](std::string value) {
    std::cout << "Received: " << value << "\n";
});
```

#### Wait for Events

```cpp
void select()
```

Continuously waits for events on the registered channels. This method runs in a loop (without busy-wait), processing available data until all channels are closed and emptied.

Use case: Continuously handle incoming data from multiple channels.

Example

```cpp
selector.select();
```

#### Stop Selector

```cpp
void stop()
```

Signals the selector to stop processing events.

Use case: Stop the selector from outside the select loop.

Example

```cpp
selector.stop();
```

## Usage Examples

### Basic Usage

```cpp
#include "channel.h"
#include <iostream>

int main() {
    Channel<int> ch(2);  // Buffered channel with capacity 2

    ch.send(1);
    ch.send(2);

    std::cout << "Received: " << *ch.receive() << std::endl;
    std::cout << "Received: " << *ch.receive() << std::endl;

    return 0;
}
```

### Asynchronous Operations

```cpp
#include "channel.h"
#include <iostream>
#include <future>

int main() {
    Channel<int> ch;

    auto future_send = ch.async_send(42);
    auto future_recv = ch.async_receive();

    future_send.wait();
    auto value = future_recv.get();

    if (value) {
        std::cout << "Received: " << *value << std::endl;
    }

    return 0;
}
```

### Using Selector

```cpp
#include "channel.h"
#include "selector.h"
#include <iostream>
#include <thread>

void producer(Channel<int>& ch, int id, int count) {
    for (int i = 0; i < count; ++i) {
        ch.send(id * 1000 + i);
    }
    ch.close();
}

void consumer(Channel<int>& ch1, Channel<int>& ch2, Selector& selector) {
    selector.add_receive<int>(ch1, [&](int value) {
        std::cout << "Channel 1 received: " << value << std::endl;
    });

    selector.add_receive<int>(ch2, [&](int value) {
        std::cout << "Channel 2 received: " << value << std::endl;
    });

    selector.select();

}

int main() {
    Channel<int> ch1(5);
    Channel<int> ch2(5);
    Selector selector;

    std::thread t1(producer, std::ref(ch1), 1, 10);
    std::thread t2(producer, std::ref(ch2), 2, 10);
    std::thread cons(consumer, std::ref(ch1), std::ref(ch2), std::ref(selector));

    t1.join();
    t2.join();

    selector.stop();

    cons.join();

    return 0;
}
```
