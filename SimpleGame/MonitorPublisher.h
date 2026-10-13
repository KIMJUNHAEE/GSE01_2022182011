#pragma once
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

class GameWorld;

// Network I/O is isolated from game state and the render thread.
// A one-slot mailbox keeps the latest snapshot without growing an offline queue.
class MonitorPublisher
{
public:

    explicit MonitorPublisher(unsigned short port = 8765);
    ~MonitorPublisher();
    MonitorPublisher(const MonitorPublisher&) = delete;
    MonitorPublisher& operator=(const MonitorPublisher&) = delete;
    void Capture(const GameWorld& world);

private:

    void Run(unsigned short port);
    std::mutex mutex;
    std::condition_variable changed;
    std::string pending;
    bool stopping = false;
    std::thread worker;
    uint64_t sequence = 0;
    std::chrono::steady_clock::time_point nextCapture = {};
};
