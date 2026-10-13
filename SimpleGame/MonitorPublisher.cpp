#include "stdafx.h"
#include "MonitorPublisher.h"
#include "Monitoring.h"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <utility>

namespace
{
    class HttpHandle
    {
    public:

        explicit HttpHandle(HINTERNET value) : value(value)
        {
        }

        ~HttpHandle()
        {
            if (value)
            {
                WinHttpCloseHandle(value);
            }
        }

        HttpHandle(const HttpHandle&) = delete;
        HttpHandle& operator=(const HttpHandle&) = delete;
        HINTERNET value;
    };

    bool Send(HINTERNET connection, const std::string& json)
    {
        HttpHandle request(WinHttpOpenRequest(connection, L"POST", L"/api/snapshot", nullptr,
                                              WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
        if (!request.value)
        {
            return false;
        }
        DWORD disabled = WINHTTP_DISABLE_REDIRECTS;
        if (!WinHttpSetOption(request.value, WINHTTP_OPTION_DISABLE_FEATURE, &disabled,
                              sizeof(disabled)))
        {
            return false;
        }
        const wchar_t* headers = L"Content-Type: application/json; charset=utf-8\r\n";
        if (!WinHttpSendRequest(request.value, headers, (DWORD)-1L, const_cast<char*>(json.data()),
                                (DWORD)json.size(), (DWORD)json.size(), 0) ||
            !WinHttpReceiveResponse(request.value, nullptr))
        {
            return false;
        }
        DWORD status = 0, size = sizeof(status);
        return WinHttpQueryHeaders(
                   request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                   WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) &&
               status == 202;
    }
} // namespace

MonitorPublisher::MonitorPublisher(unsigned short port) : worker(&MonitorPublisher::Run, this, port)
{
}

MonitorPublisher::~MonitorPublisher()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
        pending.clear();
    }
    changed.notify_one();
    if (worker.joinable())
    {
        worker.join();
    }
}

void MonitorPublisher::Capture(const GameWorld& world)
{
    const auto now = std::chrono::steady_clock::now();
    if (now < nextCapture)
    {
        return;
    }
    nextCapture = now + std::chrono::milliseconds(500);
    std::string json = BuildMonitoringSnapshot(world, ++sequence);
    {
        std::lock_guard<std::mutex> lock(mutex);
        pending = std::move(json);
    }
    changed.notify_one();
}

void MonitorPublisher::Run(unsigned short port)
{
    HttpHandle session(WinHttpOpen(L"StarDrifterMonitor/1", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value || !WinHttpSetTimeouts(session.value, 500, 500, 500, 500))
    {
        std::cerr << "[Monitor] HTTP initialization failed. Gameplay is unaffected.\n";
        return;
    }
    HttpHandle connection(WinHttpConnect(session.value, L"127.0.0.1", port, 0));
    if (!connection.value)
    {
        return;
    }
    bool connected = false, reported = false;
    for (;;)
    {
        std::string json;
        {
            std::unique_lock<std::mutex> lock(mutex);
            changed.wait(lock,
                         [this]
                         {
                             return stopping || !pending.empty();
                         });
            if (stopping)
            {
                return;
            }
            json.swap(pending);
        }
        const bool sent = Send(connection.value, json);
        if (!reported || sent != connected)
        {
            std::cout << (sent ? "[Monitor] Connected to http://127.0.0.1:"
                               : "[Monitor] Receiver unavailable at http://127.0.0.1:")
                      << port << (sent ? "/\n" : "/ (will retry; game continues)\n");
            reported = true;
        }
        connected = sent;
    }
}
